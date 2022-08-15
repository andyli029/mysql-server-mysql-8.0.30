/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright 2016 ScyllaDB
 * Copyright (c) 2022 StoneAtom, Inc. All rights reserved.
 */
#ifndef TIANMU_BASE_FILE_IMPL_H_
#define TIANMU_BASE_FILE_IMPL_H_
#pragma once

#include <atomic>
#include <deque>

#include "base/core/file.h"

namespace Tianmu {
namespace base {

class posix_file_handle_impl : public base::file_handle_impl {
  int _fd;
  std::atomic<unsigned> *_refcount;

 public:
  posix_file_handle_impl(int fd, std::atomic<unsigned> *refcount) : _fd(fd), _refcount(refcount) {}
  virtual ~posix_file_handle_impl();
  posix_file_handle_impl(const posix_file_handle_impl &) = delete;
  posix_file_handle_impl(posix_file_handle_impl &&) = delete;
  virtual shared_ptr<file_impl> to_file() && override;
  virtual std::unique_ptr<base::file_handle_impl> clone() const override;
};

class posix_file_impl : public file_impl {
  std::atomic<unsigned> *_refcount = nullptr;

 public:
  int _fd;
  posix_file_impl(int fd, file_open_options options);
  posix_file_impl(int fd, std::atomic<unsigned> *refcount);
  virtual ~posix_file_impl() override;
  future<size_t> write_dma(uint64_t pos, const void *buffer, size_t len, const io_priority_class &pc) override;
  future<size_t> write_dma(uint64_t pos, std::vector<iovec> iov, const io_priority_class &pc) override;
  future<size_t> read_dma(uint64_t pos, void *buffer, size_t len, const io_priority_class &pc) override;
  future<size_t> read_dma(uint64_t pos, std::vector<iovec> iov, const io_priority_class &pc) override;
  future<> flush(void) override;
  future<struct stat> stat(void) override;
  future<> truncate(uint64_t length) override;
  future<> discard(uint64_t offset, uint64_t length) override;
  virtual future<> allocate(uint64_t position, uint64_t length) override;
  future<uint64_t> size() override;
  virtual future<> close() noexcept override;
  virtual std::unique_ptr<base::file_handle_impl> dup() override;
  virtual subscription<directory_entry> list_directory(std::function<future<>(directory_entry de)> next) override;
  future<temporary_buffer<uint8_t>> dma_read_bulk(uint64_t offset, size_t range_size,
                                                  const io_priority_class &pc) override;

 private:
  void query_dma_alignment();

  /**
   * Try to read from the given position where the previous short read has
   * stopped. Check the EOF condition.
   *
   * The below code assumes the following: short reads due to I/O errors
   * always end at address aligned to HW block boundary. Therefore if we issue
   * a new read operation from the next position we are promised to get an
   * error (different from EINVAL). If we've got a short read because we have
   * reached EOF then the above read would either return a zero-length success
   * (if the file size is aligned to HW block size) or an EINVAL error (if
   * file length is not aligned to HW block size).
   *
   * @param pos offset to read from
   * @param len number of bytes to read
   * @param pc the IO priority class under which to queue this operation
   *
   * @return temporary buffer with read data or zero-sized temporary buffer if
   *         pos is at or beyond EOF.
   * @throw appropriate exception in case of I/O error.
   */
  future<temporary_buffer<uint8_t>> read_maybe_eof(uint64_t pos, size_t len, const io_priority_class &pc);
};

// The Linux XFS implementation is challenged wrt. append: a write that changes
// eof will be blocked by any other concurrent AIO operation to the same file,
// whether it changes file size or not. Furthermore, ftruncate() will also block
// and be blocked by AIO, so attempts to game the system and call ftruncate()
// have to be done very carefully.
//
// Other Linux filesystems may have different locking rules, so this may need to
// be adjusted for them.
class append_challenged_posix_file_impl : public posix_file_impl {
  // File size as a result of completed kernel operations (writes and truncates)
  uint64_t _committed_size;
  // File size as a result of seastar API calls
  uint64_t _logical_size;

  // Pending operations
  enum class opcode {
    invalid,
    read,
    write,
    truncate,
    flush,
  };

  struct op {
    opcode type;
    uint64_t pos;
    size_t len;
    std::function<future<>()> run;
  };

  // Queue of pending operations; processed from front to end to avoid
  // starvation, but can issue concurrent operations.
  std::deque<op> _q;
  unsigned _max_size_changing_ops = 0;
  unsigned _current_non_size_changing_ops = 0;
  unsigned _current_size_changing_ops = 0;
  // Set when the user closes the file
  bool _done = false;
  bool _sloppy_size = false;
  // Fulfiled when _done and I/O is complete
  promise<> _completed;

 private:
  void commit_size(uint64_t size) noexcept;
  bool must_run_alone(const op &candidate) const noexcept;
  bool size_changing(const op &candidate) const noexcept;
  bool may_dispatch(const op &candidate) const noexcept;
  void dispatch(op &candidate) noexcept;
  void optimize_queue() noexcept;
  void process_queue() noexcept;
  bool may_quit() const noexcept;
  void enqueue(op &&op);

 public:
  append_challenged_posix_file_impl(int fd, file_open_options options, unsigned max_size_changing_ops);
  ~append_challenged_posix_file_impl() override;

  future<size_t> read_dma(uint64_t pos, void *buffer, size_t len, const io_priority_class &pc) override;
  future<size_t> read_dma(uint64_t pos, std::vector<iovec> iov, const io_priority_class &pc) override;

  future<size_t> write_dma(uint64_t pos, const void *buffer, size_t len, const io_priority_class &pc) override;
  future<size_t> write_dma(uint64_t pos, std::vector<iovec> iov, const io_priority_class &pc) override;

  future<> flush() override;
  future<struct stat> stat() override;
  future<> truncate(uint64_t length) override;
  future<uint64_t> size() override;
  future<> close() noexcept override;
};

class blockdev_file_impl : public posix_file_impl {
 public:
  blockdev_file_impl(int fd, file_open_options options);
  future<> truncate(uint64_t length) override;
  future<> discard(uint64_t offset, uint64_t length) override;
  future<uint64_t> size() override;
  virtual future<> allocate(uint64_t position, uint64_t length) override;
};

}  // namespace base
}  // namespace Tianmu

#endif  // TIANMU_BASE_FILE_IMPL_H_
