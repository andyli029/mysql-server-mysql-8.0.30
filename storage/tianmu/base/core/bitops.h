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
#ifndef TIANMU_BASE_BITOPS_H_
#define TIANMU_BASE_BITOPS_H_
#pragma once

#include <limits>

namespace Tianmu {
namespace base {

inline constexpr unsigned count_leading_zeros(unsigned x) { return __builtin_clz(x); }

inline constexpr unsigned count_leading_zeros(unsigned long x) { return __builtin_clzl(x); }

inline constexpr unsigned count_leading_zeros(unsigned long long x) { return __builtin_clzll(x); }

inline constexpr unsigned count_trailing_zeros(unsigned x) { return __builtin_ctz(x); }

inline constexpr unsigned count_trailing_zeros(unsigned long x) { return __builtin_ctzl(x); }

inline constexpr unsigned count_trailing_zeros(unsigned long long x) { return __builtin_ctzll(x); }

template <typename T>
// requires stdx::is_integral_v<T>
inline constexpr unsigned log2ceil(T n) {
  return std::numeric_limits<T>::digits - count_leading_zeros(n - 1);
}

template <typename T>
// requires stdx::is_integral_v<T>
inline constexpr unsigned log2floor(T n) {
  return std::numeric_limits<T>::digits - count_leading_zeros(n) - 1;
}

}  // namespace base
}  // namespace Tianmu

#endif  // TIANMU_BASE_BITOPS_H
