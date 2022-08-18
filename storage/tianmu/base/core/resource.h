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
#ifndef TIANMU_BASE_RESOURCE_H_
#define TIANMU_BASE_RESOURCE_H_
#pragma once

#include <sched.h>
#include <boost/any.hpp>
#include <cstdlib>
#include <experimental/optional>
#include <set>
#include <string>
#include <vector>

namespace Tianmu {
namespace base {

cpu_set_t cpuid_to_cpuset(unsigned cpuid);

namespace resource {
using std::experimental::optional;

using cpuset = std::set<unsigned>;

struct configuration {
  optional<size_t> total_memory;
  optional<size_t> reserve_memory;  // if total_memory not specified
  optional<size_t> cpus;
  optional<cpuset> cpu_set;
  optional<unsigned> max_io_requests;
  optional<unsigned> io_queues;
};

struct memory {
  size_t bytes;
  unsigned nodeid;
};

struct io_queue {
  unsigned id;
  unsigned capacity;
};

// Since this is static information, we will keep a copy at each CPU.
// This will allow us to easily find who is the IO coordinator for a given
// node without a trip to a remote CPU.
struct io_queue_topology {
  std::vector<unsigned> shard_to_coordinator;
  std::vector<io_queue> coordinators;
};

struct cpu {
  unsigned cpu_id;
  std::vector<memory> mem;
};

struct resources {
  std::vector<cpu> cpus;
  io_queue_topology io_queues;
};

resources allocate(configuration c);
unsigned nr_processing_units();

}  // namespace resource
}  // namespace base
}  // namespace Tianmu

#endif  // TIANMU_BASE_RESOURCE_H_
