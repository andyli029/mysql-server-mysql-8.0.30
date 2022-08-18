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
 * Copyright (C) 2014 Cloudius Systems, Ltd.
 * Copyright (c) 2022 StoneAtom, Inc. All rights reserved.
 */
#ifndef TIANMU_BASE_QUEUE_H_
#define TIANMU_BASE_QUEUE_H_
#pragma once

#include <experimental/optional>
#include <queue>

#include "core/circular_buffer.h"
#include "core/future.h"

namespace Tianmu {
namespace base {

template <typename T>
class queue {
  std::queue<T, circular_buffer<T>> _q;
  size_t _max;
  std::experimental::optional<promise<>> _not_empty;
  std::experimental::optional<promise<>> _not_full;
  std::exception_ptr _ex = nullptr;

 private:
  void notify_not_empty();
  void notify_not_full();

 public:
  explicit queue(size_t size);

  // Push an item.
  //
  // Returns false if the queue was full and the item was not pushed.
  bool push(T &&a);

  // pops an item.
  T pop();

  // Consumes items from the queue, passing them to @func, until @func
  // returns false or the queue it empty
  //
  // Returns false if func returned false.
  template <typename Func>
  bool consume(Func &&func);

  // Returns true when the queue is empty.
  bool empty() const;

  // Returns true when the queue is full.
  bool full() const;

  // Returns a future<> that becomes available when pop() or consume()
  // can be called.
  future<> not_empty();

  // Returns a future<> that becomes available when push() can be called.
  future<> not_full();

  // Pops element now or when ther is some. Returns a future that becomes
  // available when some element is available.
  future<T> pop_eventually();

  future<> push_eventually(T &&data);

  size_t size() const { return _q.size(); }

  size_t max_size() const { return _max; }

  // Pushes the element now or when there is room. Returns a future<> which
  // resolves when data was pushed.
  // Set the maximum size to a new value. If the queue's max size is reduced,
  // items already in the queue will not be expunged and the queue will be
  // temporarily bigger than its max_size.
  void set_max_size(size_t max) {
    _max = max;
    if (!full()) {
      notify_not_full();
    }
  }

  // Destroy any items in the queue, and pass the provided exception to any
  // waiting readers or writers.
  void abort(std::exception_ptr ex) {
    while (!_q.empty()) {
      _q.pop();
    }
    _ex = ex;
    if (_not_full) {
      _not_full->set_exception(ex);
      _not_full = std::experimental::nullopt;
    }
    if (_not_empty) {
      _not_empty->set_exception(std::move(ex));
      _not_empty = std::experimental::nullopt;
    }
  }
};

template <typename T>
inline queue<T>::queue(size_t size) : _max(size) {}

template <typename T>
inline void queue<T>::notify_not_empty() {
  if (_not_empty) {
    _not_empty->set_value();
    _not_empty = std::experimental::optional<promise<>>();
  }
}

template <typename T>
inline void queue<T>::notify_not_full() {
  if (_not_full) {
    _not_full->set_value();
    _not_full = std::experimental::optional<promise<>>();
  }
}

template <typename T>
inline bool queue<T>::push(T &&data) {
  if (_q.size() < _max) {
    _q.push(std::move(data));
    notify_not_empty();
    return true;
  } else {
    return false;
  }
}

template <typename T>
inline T queue<T>::pop() {
  if (_q.size() == _max) {
    notify_not_full();
  }
  T data = std::move(_q.front());
  _q.pop();
  return data;
}

template <typename T>
inline future<T> queue<T>::pop_eventually() {
  if (empty()) {
    return not_empty().then([this] {
      if (_ex) {
        return make_exception_future<T>(_ex);
      } else {
        return make_ready_future<T>(pop());
      }
    });
  } else {
    return make_ready_future<T>(pop());
  }
}

template <typename T>
inline future<> queue<T>::push_eventually(T &&data) {
  if (full()) {
    return not_full().then([this, data = std::move(data)]() mutable {
      _q.push(std::move(data));
      notify_not_empty();
    });
  } else {
    _q.push(std::move(data));
    notify_not_empty();
    return make_ready_future<>();
  }
}

template <typename T>
template <typename Func>
inline bool queue<T>::consume(Func &&func) {
  bool running = true;
  while (!_q.empty() && running) {
    running = func(std::move(_q.front()));
    _q.pop();
  }
  if (!full()) {
    notify_not_full();
  }
  return running;
}

template <typename T>
inline bool queue<T>::empty() const {
  return _q.empty();
}

template <typename T>
inline bool queue<T>::full() const {
  return _q.size() >= _max;
}

template <typename T>
inline future<> queue<T>::not_empty() {
  if (!empty()) {
    return make_ready_future<>();
  } else {
    _not_empty = promise<>();
    return _not_empty->get_future();
  }
}

template <typename T>
inline future<> queue<T>::not_full() {
  if (!full()) {
    return make_ready_future<>();
  } else {
    _not_full = promise<>();
    return _not_full->get_future();
  }
}

}  // namespace base
}  // namespace Tianmu

#endif  // TIANMU_BASE_QUEUE_H_
