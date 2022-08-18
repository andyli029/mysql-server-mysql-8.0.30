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
 * Copyright 2015 Cloudius Systems
 * Copyright (c) 2022 StoneAtom, Inc. All rights reserved.
 */
#ifndef TIANMU_BASE_FILE_H_
#define TIANMU_BASE_FILE_H_
#pragma once

#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <experimental/optional>
#include <system_error>

#include "base/core/align.h"
#include "base/core/fair_queue.h"
#include "base/core/future_util.h"
#include "base/core/shared_ptr.h"
#include "base/core/sstring.h"
#include "base/core/stream.h"

namespace Tianmu {
namespace base {

/// \addtogroup fileio-module
/// @{

/// Enumeration describing the type of a directory entry being listed.
///
/// \see file::list_directory()
enum class directory_entry_type {
  block_device,
  char_device,
  directory,
  fifo,
  link,
  regular,
  socket,
};

/// Enumeration describing the type of a particular filesystem
enum class fs_type {
  other,
  xfs,
  ext2,
  ext3,
  ext4,
  btrfs,
  hfs,
  tmpfs,
};

/// A directory entry being listed.
struct directory_entry {
  /// Name of the file in a directory entry.  Will never be "." or "..".  Only
  /// the last component is included.
  sstring name;
  /// Type of the directory entry, if known.
  std::experimental::optional<directory_entry_type> type;
};

/// File open options
///
/// Options used to configure an open file.
///
/// \ref file
struct file_open_options {
  uint64_t extent_allocation_size_hint = 1 << 20;  ///< Allocate this much disk space when extending the file
  bool sloppy_size = false;                        ///< Allow the file size not to track the amount of
                                                   ///< data written until a flush
  uint64_t sloppy_size_hint = 1 << 20;             ///< Hint as to what the eventual file size will be
};

/// \cond internal
class io_queue;
class io_priority_class {
  unsigned val;
  friend io_queue;

 public:
  unsigned id() const { return val; }
};

const io_priority_class &default_priority_class();

class file;
class file_impl;

class file_handle;

// A handle that can be transported across shards and used to
// create a dup(2)-like `file` object referring to the same underlying file
class file_handle_impl {
 public:
  virtual ~file_handle_impl() = default;
  virtual std::unique_ptr<file_handle_impl> clone() const = 0;
  virtual shared_ptr<file_impl> to_file() && = 0;
};

class file_impl {
 protected:
  static file_impl *get_file_impl(file &f);

 public:
  unsigned _memory_dma_alignment = 4096;
  unsigned _disk_read_dma_alignment = 4096;
  unsigned _disk_write_dma_alignment = 4096;

 public:
  virtual ~file_impl() {}

  virtual future<size_t> write_dma(uint64_t pos, const void *buffer, size_t len, const io_priority_class &pc) = 0;
  virtual future<size_t> write_dma(uint64_t pos, std::vector<iovec> iov, const io_priority_class &pc) = 0;
  virtual future<size_t> read_dma(uint64_t pos, void *buffer, size_t len, const io_priority_class &pc) = 0;
  virtual future<size_t> read_dma(uint64_t pos, std::vector<iovec> iov, const io_priority_class &pc) = 0;
  virtual future<> flush(void) = 0;
  virtual future<struct stat> stat(void) = 0;
  virtual future<> truncate(uint64_t length) = 0;
  virtual future<> discard(uint64_t offset, uint64_t length) = 0;
  virtual future<> allocate(uint64_t position, uint64_t length) = 0;
  virtual future<uint64_t> size(void) = 0;
  virtual future<> close() = 0;
  virtual std::unique_ptr<file_handle_impl> dup();
  virtual subscription<directory_entry> list_directory(std::function<future<>(directory_entry de)> next) = 0;
  virtual future<temporary_buffer<uint8_t>> dma_read_bulk(uint64_t offset, size_t range_size,
                                                          const io_priority_class &pc) = 0;

  friend class reactor;
};

/// \endcond

/// A data file on persistent storage.
///
/// File objects represent uncached, unbuffered files.  As such great care
/// must be taken to cache data at the application layer; neither seastar
/// nor the OS will cache these file.
///
/// Data is transferred using direct memory access (DMA).  This imposes
/// restrictions on file offsets and data pointers.  The former must be aligned
/// on a 4096 byte boundary, while a 512 byte boundary suffices for the latter.
class file {
  shared_ptr<file_impl> _file_impl;

 private:
  explicit file(int fd, file_open_options options);

 public:
  /// Default constructor constructs an uninitialized file object.
  ///
  /// A default constructor is useful for the common practice of declaring
  /// a variable, and only assigning to it later. The uninitialized file
  /// must not be used, or undefined behavior will result (currently, a null
  /// pointer dereference).
  ///
  /// One can check whether a file object is in uninitialized state with
  /// \ref operator bool(); One can reset a file back to uninitialized state
  /// by assigning file() to it.
  file() : _file_impl(nullptr) {}

  file(shared_ptr<file_impl> impl) : _file_impl(std::move(impl)) {}

  /// Constructs a file object from a \ref file_handle obtained from another
  /// shard
  explicit file(file_handle &&handle);

  /// Checks whether the file object was initialized.
  ///
  /// \return false if the file object is uninitialized (default
  /// constructed), true if the file object refers to an actual file.
  explicit operator bool() const noexcept { return bool(_file_impl); }

  /// Copies a file object.  The new and old objects refer to the
  /// same underlying file.
  ///
  /// \param x file object to be copied
  file(const file &x) = default;
  /// Moves a file object.
  file(file &&x) noexcept : _file_impl(std::move(x._file_impl)) {}
  /// Assigns a file object.  After assignent, the destination and source refer
  /// to the same underlying file.
  ///
  /// \param x file object to assign to `this`.
  file &operator=(const file &x) noexcept = default;
  /// Moves assigns a file object.
  file &operator=(file &&x) noexcept = default;

  // O_DIRECT reading requires that buffer, offset, and read length, are
  // all aligned. Alignment of 4096 was necessary in the past, but no longer
  // is - 512 is usually enough; But we'll need to use BLKSSZGET ioctl to
  // be sure it is really enough on this filesystem. 4096 is always safe.
  // In addition, if we start reading in things outside page boundaries,
  // we will end up with various pages around, some of them with
  // overlapping ranges. Those would be very challenging to cache.

  /// Alignment requirement for file offsets (for reads)
  uint64_t disk_read_dma_alignment() const { return _file_impl->_disk_read_dma_alignment; }

  /// Alignment requirement for file offsets (for writes)
  uint64_t disk_write_dma_alignment() const { return _file_impl->_disk_write_dma_alignment; }

  /// Alignment requirement for data buffers
  uint64_t memory_dma_alignment() const { return _file_impl->_memory_dma_alignment; }

  /**
   * Perform a single DMA read operation.
   *
   * @param aligned_pos offset to begin reading at (should be aligned)
   * @param aligned_buffer output buffer (should be aligned)
   * @param aligned_len number of bytes to read (should be aligned)
   * @param pc the IO priority class under which to queue this operation
   *
   * Alignment is HW dependent but use 4KB alignment to be on the safe side as
   * explained above.
   *
   * @return number of bytes actually read
   * @throw exception in case of I/O error
   */
  template <typename CharType>
  future<size_t> dma_read(uint64_t aligned_pos, CharType *aligned_buffer, size_t aligned_len,
                          const io_priority_class &pc = default_priority_class()) {
    return _file_impl->read_dma(aligned_pos, aligned_buffer, aligned_len, pc);
  }

  /**
   * Read the requested amount of bytes starting from the given offset.
   *
   * @param pos offset to begin reading from
   * @param len number of bytes to read
   * @param pc the IO priority class under which to queue this operation
   *
   * @return temporary buffer containing the requested data.
   * @throw exception in case of I/O error
   *
   * This function doesn't require any alignment for both "pos" and "len"
   *
   * @note size of the returned buffer may be smaller than "len" if EOF is
   *       reached of in case of I/O error.
   */
  template <typename CharType>
  future<temporary_buffer<CharType>> dma_read(uint64_t pos, size_t len,
                                              const io_priority_class &pc = default_priority_class()) {
    return dma_read_bulk<CharType>(pos, len, pc).then([len](temporary_buffer<CharType> buf) {
      if (len < buf.size()) {
        buf.trim(len);
      }

      return std::move(buf);
    });
  }

  /// Error thrown when attempting to read past end-of-file
  /// with \ref dma_read_exactly().
  class eof_error : public std::exception {};

  /**
   * Read the exact amount of bytes.
   *
   * @param pos offset in a file to begin reading from
   * @param len number of bytes to read
   * @param pc the IO priority class under which to queue this operation
   *
   * @return temporary buffer containing the read data
   * @throw end_of_file_error if EOF is reached, file_io_error or
   *        std::system_error in case of I/O error.
   */
  template <typename CharType>
  future<temporary_buffer<CharType>> dma_read_exactly(uint64_t pos, size_t len,
                                                      const io_priority_class &pc = default_priority_class()) {
    return dma_read<CharType>(pos, len, pc).then([pos, len](auto buf) {
      if (buf.size() < len) {
        throw eof_error();
      }

      return std::move(buf);
    });
  }

  /// Performs a DMA read into the specified iovec.
  ///
  /// \param pos offset to read from.  Must be aligned to \ref dma_alignment.
  /// \param iov vector of address/size pairs to read into.  Addresses must be
  ///            aligned.
  /// \param pc the IO priority class under which to queue this operation
  ///
  /// \return a future representing the number of bytes actually read.  A short
  ///         read may happen due to end-of-file or an I/O error.
  future<size_t> dma_read(uint64_t pos, std::vector<iovec> iov,
                          const io_priority_class &pc = default_priority_class()) {
    return _file_impl->read_dma(pos, std::move(iov), pc);
  }

  /// Performs a DMA write from the specified buffer.
  ///
  /// \param pos offset to write into.  Must be aligned to \ref dma_alignment.
  /// \param buffer aligned address of buffer to read from.  Buffer must exists
  ///               until the future is made ready.
  /// \param len number of bytes to write.  Must be aligned.
  /// \param pc the IO priority class under which to queue this operation
  ///
  /// \return a future representing the number of bytes actually written.  A
  /// short
  ///         write may happen due to an I/O error.
  template <typename CharType>
  future<size_t> dma_write(uint64_t pos, const CharType *buffer, size_t len,
                           const io_priority_class &pc = default_priority_class()) {
    return _file_impl->write_dma(pos, buffer, len, pc);
  }

  /// Performs a DMA write to the specified iovec.
  ///
  /// \param pos offset to write into.  Must be aligned to \ref dma_alignment.
  /// \param iov vector of address/size pairs to write from.  Addresses must be
  ///            aligned.
  /// \param pc the IO priority class under which to queue this operation
  ///
  /// \return a future representing the number of bytes actually written.  A
  /// short
  ///         write may happen due to an I/O error.
  future<size_t> dma_write(uint64_t pos, std::vector<iovec> iov,
                           const io_priority_class &pc = default_priority_class()) {
    return _file_impl->write_dma(pos, std::move(iov), pc);
  }

  /// Causes any previously written data to be made stable on persistent
  /// storage.
  ///
  /// Prior to a flush, written data may or may not survive a power failure.
  /// After a flush, data is guaranteed to be on disk.
  future<> flush() { return _file_impl->flush(); }

  /// Returns \c stat information about the file.
  future<struct stat> stat() { return _file_impl->stat(); }

  /// Truncates the file to a specified length.
  future<> truncate(uint64_t length) { return _file_impl->truncate(length); }

  /// Preallocate disk blocks for a specified byte range.
  ///
  /// Requests the file system to allocate disk blocks to
  /// back the specified range (\c length bytes starting at
  /// \c position).  The range may be outside the current file
  /// size; the blocks can then be used when appending to the
  /// file.
  ///
  /// \param position beginning of the range at which to allocate
  ///                 blocks.
  /// \parm length length of range to allocate.
  /// \return future that becomes ready when the operation completes.
  future<> allocate(uint64_t position, uint64_t length) { return _file_impl->allocate(position, length); }

  /// Discard unneeded data from the file.
  ///
  /// The discard operation tells the file system that a range of offsets
  /// (which be aligned) is no longer needed and can be reused.
  future<> discard(uint64_t offset, uint64_t length) { return _file_impl->discard(offset, length); }

  /// Gets the file size.
  future<uint64_t> size() const { return _file_impl->size(); }

  /// Closes the file.
  ///
  /// Flushes any pending operations and release any resources associated with
  /// the file (except for stable storage).
  ///
  /// \note
  /// to ensure file data reaches stable storage, you must call \ref flush()
  /// before calling \c close().
  future<> close() { return _file_impl->close(); }

  /// Returns a directory listing, given that this file object is a directory.
  subscription<directory_entry> list_directory(std::function<future<>(directory_entry de)> next) {
    return _file_impl->list_directory(std::move(next));
  }

  /**
   * Read a data bulk containing the provided addresses range that starts at
   * the given offset and ends at either the address aligned to
   * dma_alignment (4KB) or at the file end.
   *
   * @param offset starting address of the range the read bulk should contain
   * @param range_size size of the addresses range
   * @param pc the IO priority class under which to queue this operation
   *
   * @return temporary buffer containing the read data bulk.
   * @throw system_error exception in case of I/O error or eof_error when
   *        "offset" is beyond EOF.
   */
  template <typename CharType>
  future<temporary_buffer<CharType>> dma_read_bulk(uint64_t offset, size_t range_size,
                                                   const io_priority_class &pc = default_priority_class()) {
    return _file_impl->dma_read_bulk(offset, range_size, pc).then([](temporary_buffer<uint8_t> t) {
      return temporary_buffer<CharType>(reinterpret_cast<CharType *>(t.get_write()), t.size(), t.release());
    });
  }

  /// \brief Creates a handle that can be transported across shards.
  ///
  /// Creates a handle that can be transported across shards, and then
  /// used to create a new shard-local \ref file object that refers to
  /// the same on-disk file.
  ///
  /// \note Use on read-only files.
  ///
  file_handle dup();

  template <typename CharType>
  struct read_state;

 private:
  friend class reactor;
  friend class file_impl;
};

/// \brief A shard-transportable handle to a file
///
/// If you need to access a file (for reads only) across multiple shards,
/// you can use the file::dup() method to create a `file_handle`, transport
/// this file handle to another shard, and use the handle to create \ref file
/// object on that shard.  This is more efficient than calling open_file_dma()
/// again.
class file_handle {
  std::unique_ptr<file_handle_impl> _impl;

 private:
  explicit file_handle(std::unique_ptr<file_handle_impl> impl) : _impl(std::move(impl)) {}

 public:
  /// Copies a file handle object
  file_handle(const file_handle &);
  /// Moves a file handle object
  file_handle(file_handle &&) noexcept;
  /// Assigns a file handle object
  file_handle &operator=(const file_handle &);
  /// Move-assigns a file handle object
  file_handle &operator=(file_handle &&) noexcept;
  /// Converts the file handle object to a \ref file.
  file to_file() const &;
  /// Converts the file handle object to a \ref file.
  file to_file() &&;

  friend class file;
};

/// \cond internal

template <typename CharType>
struct file::read_state {
  using tmp_buf_type = temporary_buffer<CharType>;

  read_state(uint64_t offset, uint64_t front, size_t to_read, size_t memory_alignment, size_t disk_alignment)
      : buf(tmp_buf_type::aligned(memory_alignment, align_up(to_read, disk_alignment))),
        _offset(offset),
        _to_read(to_read),
        _front(front) {}

  bool done() const { return eof || pos >= _to_read; }

  /**
   * Trim the buffer to the actual number of read bytes and cut the
   * bytes from offset 0 till "_front".
   *
   * @note this function has to be called only if we read bytes beyond
   *       "_front".
   */
  void trim_buf_before_ret() {
    if (have_good_bytes()) {
      buf.trim(pos);
      buf.trim_front(_front);
    } else {
      buf.trim(0);
    }
  }

  uint64_t cur_offset() const { return _offset + pos; }

  size_t left_space() const { return buf.size() - pos; }

  size_t left_to_read() const {
    // positive as long as (done() == false)
    return _to_read - pos;
  }

  void append_new_data(tmp_buf_type &new_data) {
    auto to_copy = std::min(left_space(), new_data.size());

    std::memcpy(buf.get_write() + pos, new_data.get(), to_copy);
    pos += to_copy;
  }

  bool have_good_bytes() const { return pos > _front; }

 public:
  bool eof = false;
  tmp_buf_type buf;
  size_t pos = 0;

 private:
  uint64_t _offset;
  size_t _to_read;
  uint64_t _front;
};

/// \endcond

/// @}

}  // namespace base
}  // namespace Tianmu

#endif  // TIANMU_BASE_FILE_H_
