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
 * Copyright (C) 2015 Cloudius Systems, Ltd.
 * Copyright (c) 2022 StoneAtom, Inc. All rights reserved.
 */

#ifndef TIANMU_BASE_SLEEP_H_
#define TIANMU_BASE_SLEEP_H_
#pragma once

#include <chrono>
#include <functional>

#include "base/core/future.h"
#include "base/core/reactor.h"
#include "base/core/shared_ptr.h"

namespace Tianmu {
namespace base {

/// Returns a future which completes after a specified time has elapsed.
///
/// \param dur minimum amount of time before the returned future becomes
///            ready.
/// \return A \ref future which becomes ready when the sleep duration elapses.
template <typename Clock = steady_clock_type, typename Rep, typename Period>
future<> sleep(std::chrono::duration<Rep, Period> dur) {
  struct sleeper {
    promise<> done;
    timer<Clock> tmr;
    sleeper(std::chrono::duration<Rep, Period> dur) : tmr([this] { done.set_value(); }) { tmr.arm(dur); }
  };
  sleeper *s = new sleeper(dur);
  future<> fut = s->done.get_future();
  return fut.then([s] { delete s; });
}

/// exception that is thrown when application is in process of been stopped
class sleep_aborted : public std::exception {
 public:
  /// Reports the exception reason.
  virtual const char *what() const noexcept { return "Sleep is aborted"; }
};

/// Returns a future which completes after a specified time has elapsed
/// or throws \ref sleep_aborted exception if application is aborted
///
/// \param dur minimum amount of time before the returned future becomes
///            ready.
/// \return A \ref future which becomes ready when the sleep duration elapses.
template <typename Rep, typename Period>
future<> sleep_abortable(std::chrono::duration<Rep, Period> dur) {
  return engine().wait_for_stop(dur).then([] { throw sleep_aborted(); }).handle_exception([](std::exception_ptr ep) {
    try {
      std::rethrow_exception(ep);
    } catch (condition_variable_timed_out &) {
    };
  });
}

}  // namespace base
}  // namespace Tianmu

#endif  // TIANMU_BASE_SLEEP_H_
