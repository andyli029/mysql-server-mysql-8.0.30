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
 * Copyright (C) 2016 ScyllaDB.
 * Copyright (c) 2022 StoneAtom, Inc. All rights reserved.
 */
#ifndef TIANMU_BASE_PREEMPT_H_
#define TIANMU_BASE_PREEMPT_H_
#pragma once
#include <atomic>

namespace Tianmu {
namespace base {

extern __thread bool g_need_preempt;

inline bool need_preempt() {
#ifndef DEBUG
  // prevent compiler from eliminating loads in a loop
  std::atomic_signal_fence(std::memory_order_seq_cst);
  return g_need_preempt;
#else
  return true;
#endif
}

}  // namespace base
}  // namespace Tianmu

#endif  // TIANMU_BASE_PREEMPT_H_
