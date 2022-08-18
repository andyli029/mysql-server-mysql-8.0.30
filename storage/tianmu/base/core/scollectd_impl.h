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
 * Copyright (C) 2016 ScyllaDB
 * Copyright (c) 2022 StoneAtom, Inc. All rights reserved.
 */

#ifndef TIANMU_BASE_SCOLLECTD_IMPL_H_
#define TIANMU_BASE_SCOLLECTD_IMPL_H_
#pragma once

#include "core/metrics_api.h"
#include "core/reactor.h"
#include "core/scollectd.h"

namespace Tianmu {
namespace base {
namespace scollectd {

using namespace std::chrono_literals;
using duration = std::chrono::milliseconds;

static const ipv4_addr default_addr("239.192.74.66:25826");
static const std::chrono::milliseconds default_period(1s);

class impl {
  net::udp_channel _chan;
  timer<> _timer;

  sstring _host = "localhost";
  ipv4_addr _addr = default_addr;
  std::chrono::milliseconds _period = default_period;
  uint64_t _num_packets = 0;
  uint64_t _millis = 0;
  uint64_t _bytes = 0;
  double _avg = 0;

 public:
  using value_list_map = base::metrics::impl::value_map;
  using value_list_pair = value_list_map::value_type;

  void add_polled(const type_instance_id &id, const shared_ptr<value_list> &values, bool enable = true);
  void remove_polled(const type_instance_id &id);
  // explicitly send a type_instance value list (outside polling)
  future<> send_metric(const type_instance_id &id, const value_list &values);
  future<> send_notification(const type_instance_id &id, const sstring &msg);
  // initiates actual value polling -> send to target "loop"
  void start(const sstring &host, const ipv4_addr &addr, const std::chrono::milliseconds period);
  void stop();

  value_list_map &get_value_list_map();
  const sstring &host() const { return _host; }

 private:
  void arm();
  void run();

 public:
  shared_ptr<value_list> get_values(const type_instance_id &id) const;
  std::vector<type_instance_id> get_instance_ids() const;
  sstring get_collectd_description_str(const scollectd::type_instance_id &) const;

 private:
  const value_list_map &values() const { return base::metrics::impl::get_value_map(); }
  base::metrics::metric_groups _metrics;
};

impl &get_impl();

}  // namespace scollectd
}  // namespace base
}  // namespace Tianmu

#endif  // TIANMU_BASE_SCOLLECTD_IMPL_H_
