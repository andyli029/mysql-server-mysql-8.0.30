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
 * Copyright (C) 2017 ScyllaDB
 * Copyright (c) 2022 StoneAtom, Inc. All rights reserved.
 */
#ifndef TIANMU_BASE_LOG_CLI_H_
#define TIANMU_BASE_LOG_CLI_H_
#pragma once

#include <algorithm>
#include <iostream>
#include <unordered_map>

#include "base/core/sstring.h"
#include "base/util/log.h"
#include "base/util/program_options.h"

/// \addtogroup logging
/// @{

namespace Tianmu {
namespace base {

/// \brief Configure application logging at run-time with program options.

namespace log_cli {
/// \brief Print a human-friendly list of the available loggers.
void print_available_loggers(std::ostream &os);

/// \brief Parse a log-level ({error, warn, info, debug, trace}) string,
/// throwing \c std::runtime_error for an invalid level.
log_level parse_log_level(const sstring &);

// \brief Parse associations from loggers to log-levels and write the resulting
// pairs to the output iterator.
// \throws \c std::runtime_error for an invalid log-level.
template <class OutputIter>
void parse_logger_levels(const program_options::string_map &levels, OutputIter out) {
  std::for_each(levels.begin(), levels.end(),
                [&out](auto &&pair) { *out++ = std::make_pair(pair.first, parse_log_level(pair.second)); });
}

logging_settings create_default_logging_settings();
}  // namespace log_cli

}  // namespace base
}  // namespace Tianmu
/// @}

#endif  // TIANMU_BASE_LOG_CLI_H_
