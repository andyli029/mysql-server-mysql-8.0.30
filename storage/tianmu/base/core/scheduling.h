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
 * Copyright (C) 2016 Scylla DB Ltd
 * Copyright (c) 2022 StoneAtom, Inc. All rights reserved.
 */
#ifndef TIANMU_BASE_SCHEDULING_H_
#define TIANMU_BASE_SCHEDULING_H_
#pragma once

#include "base/core/sstring.h"

namespace Tianmu {
namespace base {

template <typename... T>
class future;

class reactor;

class scheduling_group;

/// Creates a scheduling group with a specified number of shares.
///
/// \param name A name that identifiers the group; will be used as a label
///             in the group's metrics
/// \param shares number of shares of the CPU time allotted to the group;
///              Use numbers in the 1-1000 range (but can go above).
/// \return a scheduling group that can be used on any shard
future<scheduling_group> create_scheduling_group(sstring name, float shares);

/// \brief Identifies function calls that are accounted as a group
///
/// A `scheduling_group` is a tag that can be used to mark a function call.
/// Executions of such tagged calls are accounted as a group.
class scheduling_group {
  unsigned _id;

 private:
  explicit scheduling_group(unsigned id) : _id(id) {}

 public:
  /// Creates a `scheduling_group` object denoting the default group
  constexpr scheduling_group() noexcept : _id(0) {}  // must be constexpr for current_scheduling_group_holder
  bool active() const;
  const sstring &name() const;
  bool operator==(scheduling_group x) const { return _id == x._id; }
  bool operator!=(scheduling_group x) const { return _id != x._id; }
  bool is_main() const { return _id == 0; }
  /// Adjusts the number of shares allotted to the group.
  ///
  /// Dynamically adjust the number of shares allotted to the group, increasing
  /// or decreasing the amount of CPU bandwidth it gets. The adjustment is local
  /// to the shard.
  ///
  /// This can be used to reduce a background job's interference with a
  /// foreground load: the shares can be started at a low value, increased when
  /// the background job's backlog increases, and reduced again when the backlog
  /// decreases.
  ///
  /// \param shares number of shares allotted to the group. Use numbers
  ///               in the 1-1000 range.
  void set_shares(float shares);
  friend future<scheduling_group> create_scheduling_group(sstring name, float shares);
  friend class reactor;
};

/// \cond internal
namespace internal {
inline scheduling_group *current_scheduling_group_ptr() {
  // Slow unless constructor is constexpr
  static thread_local scheduling_group sg;
  return &sg;
}
}  // namespace internal
/// \endcond

/// Returns the current scheduling group
inline scheduling_group current_scheduling_group() { return *internal::current_scheduling_group_ptr(); }

inline scheduling_group default_scheduling_group() { return scheduling_group(); }

inline bool scheduling_group::active() const { return *this == current_scheduling_group(); }

}  // namespace base
}  // namespace Tianmu

#endif  // TIANMU_BASE_SCHEDULING_H_
