/*
 * Copyright 2021 Database Group, Nagoya University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PMWCAS_UTILITY_HPP
#define PMWCAS_UTILITY_HPP

// C++ standard libraries
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

// external sources
#include "thread/id_manager.hpp"

namespace dbgroup::atomic::pmwcas
{
/*######################################################################################
 * Global enum and constants
 *####################################################################################*/

#ifdef PMWCAS_USE_DIRTY_FLAG
/// @brief A flag to indicate the use of dirty flags.
constexpr bool kUseDirtyFlag = true;
#else
/// @brief A flag to indicate the use of dirty flags.
constexpr bool kUseDirtyFlag = false;
#endif

/// @brief The maximum number of target words of PMwCAS.
constexpr size_t kPMwCASCapacity = PMWCAS_CAPACITY;

/// @brief The maximum number of retries for preventing busy loops.
constexpr size_t kRetryNum = PMWCAS_RETRY_THRESHOLD;

/// @brief A sleep time for preventing busy loops [us].
constexpr auto kShortSleep = std::chrono::microseconds{PMWCAS_SLEEP_TIME};

/// @brief The maximum number of threads used in a process.
constexpr size_t kMaxThreadNum = ::dbgroup::thread::kMaxThreadNum;

/// @brief Assumes that the length of one word is 8 bytes
constexpr size_t kWordSize = 8;

/// @brief Assumes that the size of one cache line is 64 bytes
constexpr size_t kCacheLineSize = 64;

/// @brief Assumes that the size of one line on persistent memory is 256 bytes
constexpr size_t kPMEMLineSize = 256;

/*######################################################################################
 * Global utility functions
 *####################################################################################*/

/**
 * @tparam T a PMwCAS target class.
 * @retval true if a target class can be updated by PMwCAS.
 * @retval false otherwise.
 */
template <class T>
constexpr auto
CanPMwCAS()  //
    -> bool
{
  return std::is_same_v<T, uint64_t> || std::is_pointer_v<T>;
}

}  // namespace dbgroup::atomic::pmwcas

#endif  // PMWCAS_UTILITY_HPP
