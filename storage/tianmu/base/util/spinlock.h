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
 */
#ifndef TIANMU_BASE_SPINLOCK_H_
#define TIANMU_BASE_SPINLOCK_H_
#pragma once

#include <atomic>
#include <cassert>

#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif

namespace Tianmu {
namespace base {
namespace util {

// Spin lock implementation.
// BasicLockable.
// Async-signal safe.
// unlock() "synchronizes with" lock().
class spinlock {
  std::atomic<bool> _busy = {false};

 public:
  spinlock() = default;
  spinlock(const spinlock &other) : _busy(other._busy.load()) {}
  spinlock &operator=(const spinlock &other) {
    _busy.store(other._busy.load());
    return *this;
  }

  ~spinlock() { assert(!_busy.load(std::memory_order_relaxed)); }
  void lock() noexcept {
    while (_busy.exchange(true, std::memory_order_acquire)) {
#if defined(__x86_64__) || defined(__i386__)
      _mm_pause();
#endif
    }
  }
  void unlock() noexcept { _busy.store(false, std::memory_order_release); }
};

}  // namespace util
}  // namespace base
}  // namespace Tianmu

#endif  // TIANMU_BASE_SPINLOCK_H_
