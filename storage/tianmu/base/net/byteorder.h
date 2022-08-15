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
#ifndef TIANMU_BASE_NET_BYTEORDER_H_
#define TIANMU_BASE_NET_BYTEORDER_H_
#pragma once

#include <arpa/inet.h>  // for ntohs() and friends
#include <iosfwd>
#include <utility>

#include "base/core/unaligned.h"

namespace Tianmu {
namespace base {

inline uint64_t ntohq(uint64_t v) {
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  // big endian, nothing to do
  return v;
#else
  // little endian, reverse bytes
  return __builtin_bswap64(v);
#endif
}

inline uint64_t htonq(uint64_t v) {
  // htonq and ntohq have identical implementations
  return ntohq(v);
}

namespace net {

inline void ntoh() {}
inline void hton() {}

inline uint8_t ntoh(uint8_t x) { return x; }
inline uint8_t hton(uint8_t x) { return x; }
inline uint16_t ntoh(uint16_t x) { return ntohs(x); }
inline uint16_t hton(uint16_t x) { return htons(x); }
inline uint32_t ntoh(uint32_t x) { return ntohl(x); }
inline uint32_t hton(uint32_t x) { return htonl(x); }
inline uint64_t ntoh(uint64_t x) { return ntohq(x); }
inline uint64_t hton(uint64_t x) { return htonq(x); }

inline int8_t ntoh(int8_t x) { return x; }
inline int8_t hton(int8_t x) { return x; }
inline int16_t ntoh(int16_t x) { return ntohs(x); }
inline int16_t hton(int16_t x) { return htons(x); }
inline int32_t ntoh(int32_t x) { return ntohl(x); }
inline int32_t hton(int32_t x) { return htonl(x); }
inline int64_t ntoh(int64_t x) { return ntohq(x); }
inline int64_t hton(int64_t x) { return htonq(x); }

// Deprecated alias net::packed<> for unaligned<> from unaligned.hh.
// TODO: get rid of this alias.
template <typename T>
using packed = unaligned<T>;

template <typename T>
inline T ntoh(const packed<T> &x) {
  T v = x;
  return ntoh(v);
}

template <typename T>
inline T hton(const packed<T> &x) {
  T v = x;
  return hton(v);
}

template <typename T>
inline std::ostream &operator<<(std::ostream &os, const packed<T> &v) {
  auto x = v.raw;
  return os << x;
}

inline void ntoh_inplace() {}
inline void hton_inplace(){};

template <typename First, typename... Rest>
inline void ntoh_inplace(First &first, Rest &...rest) {
  first = ntoh(first);
  ntoh_inplace(std::forward<Rest &>(rest)...);
}

template <typename First, typename... Rest>
inline void hton_inplace(First &first, Rest &...rest) {
  first = hton(first);
  hton_inplace(std::forward<Rest &>(rest)...);
}

template <class T>
inline T ntoh(const T &x) {
  T tmp = x;
  tmp.adjust_endianness([](auto &&...what) { ntoh_inplace(std::forward<decltype(what) &>(what)...); });
  return tmp;
}

template <class T>
inline T hton(const T &x) {
  T tmp = x;
  tmp.adjust_endianness([](auto &&...what) { hton_inplace(std::forward<decltype(what) &>(what)...); });
  return tmp;
}

}  // namespace net
}  // namespace base
}  // namespace Tianmu

#endif  // TIANMU_BASE_NET_BYTEORDER_H_
