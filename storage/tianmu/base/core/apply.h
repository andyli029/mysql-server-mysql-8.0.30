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
#ifndef TIANMU_BASE_APPLY_H_
#define TIANMU_BASE_APPLY_H_
#pragma once

#include <tuple>
#include <utility>

namespace Tianmu {
namespace base {

template <typename Func, typename Args, typename IndexList>
struct apply_helper;

template <typename Func, typename Tuple, size_t... I>
struct apply_helper<Func, Tuple, std::index_sequence<I...>> {
  static auto apply(Func &&func, Tuple args) { return func(std::get<I>(std::forward<Tuple>(args))...); }
};

template <typename Func, typename... T>
inline auto apply(Func &&func, std::tuple<T...> &&args) {
  using helper = apply_helper<Func, std::tuple<T...> &&, std::index_sequence_for<T...>>;
  return helper::apply(std::forward<Func>(func), std::move(args));
}

template <typename Func, typename... T>
inline auto apply(Func &&func, std::tuple<T...> &args) {
  using helper = apply_helper<Func, std::tuple<T...> &, std::index_sequence_for<T...>>;
  return helper::apply(std::forward<Func>(func), args);
}

template <typename Func, typename... T>
inline auto apply(Func &&func, const std::tuple<T...> &args) {
  using helper = apply_helper<Func, const std::tuple<T...> &, std::index_sequence_for<T...>>;
  return helper::apply(std::forward<Func>(func), args);
}

}  // namespace base
}  // namespace Tianmu

#endif  // TIANMU_BASE_APPLY_H_
