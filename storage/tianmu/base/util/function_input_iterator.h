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
#ifndef TIANMU_BASE_FUNCTION_INPUT_ITERATOR_H_
#define TIANMU_BASE_FUNCTION_INPUT_ITERATOR_H_
#pragma once

namespace Tianmu {
namespace base {

template <typename Function, typename State>
struct function_input_iterator {
  Function _func;
  State _state;

 public:
  function_input_iterator(Function func, State state) : _func(func), _state(state) {}
  function_input_iterator(const function_input_iterator &) = default;
  function_input_iterator(function_input_iterator &&) = default;
  function_input_iterator &operator=(const function_input_iterator &) = default;
  function_input_iterator &operator=(function_input_iterator &&) = default;
  auto operator*() const { return _func(); }
  function_input_iterator &operator++() {
    ++_state;
    return *this;
  }
  function_input_iterator operator++(int) {
    function_input_iterator ret{*this};
    ++_state;
    return ret;
  }
  bool operator==(const function_input_iterator &x) const { return _state == x._state; }
  bool operator!=(const function_input_iterator &x) const { return !operator==(x); }
};

template <typename Function, typename State>
inline function_input_iterator<Function, State> make_function_input_iterator(Function func, State state) {
  return function_input_iterator<Function, State>(func, state);
}

template <typename Function, typename State>
inline function_input_iterator<Function, State> make_function_input_iterator(Function &&func) {
  return function_input_iterator<Function, State>(func, State{});
}

}  // namespace base
}  // namespace Tianmu

#endif  // TIANMU_BASE_FUNCTION_INPUT_ITERATOR_H_
