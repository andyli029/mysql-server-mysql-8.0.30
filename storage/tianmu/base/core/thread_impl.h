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

#ifndef TIANMU_BASE_THREAD_IMPL_H_
#define TIANMU_BASE_THREAD_IMPL_H_
#pragma once

#include <setjmp.h>
#include <ucontext.h>
#include <chrono>
#include <experimental/optional>

#include "base/core/preempt.h"

namespace Tianmu {
namespace base {

/// Clock used for scheduling threads
using thread_clock = std::chrono::steady_clock;

class thread_context;
class scheduling_group;

struct jmp_buf_link {
#ifdef ASAN_ENABLED
  ucontext_t context;
  void *fake_stack = nullptr;
  const void *stack_bottom;
  size_t stack_size;
#else
  jmp_buf jmpbuf;
#endif
  jmp_buf_link *link;
  thread_context *thread;
  std::experimental::optional<std::chrono::time_point<thread_clock>> yield_at = {};

 public:
  void initial_switch_in(ucontext_t *initial_context, const void *stack_bottom, size_t stack_size);
  void switch_in();
  void switch_out();
  void initial_switch_in_completed();
  void final_switch_out();
};

extern thread_local jmp_buf_link *g_current_context;

namespace thread_impl {

inline thread_context *get() { return g_current_context->thread; }

inline bool should_yield() {
  if (need_preempt()) {
    return true;
  } else if (g_current_context->yield_at) {
    return std::chrono::steady_clock::now() >= *(g_current_context->yield_at);
  } else {
    return false;
  }
}

scheduling_group sched_group(const thread_context *);

void yield();
void switch_in(thread_context *to);
void switch_out(thread_context *from);
void init();
}  // namespace thread_impl

}  // namespace base
}  // namespace Tianmu

#endif  // TIANMU_BASE_THREAD_IMPL_H_
