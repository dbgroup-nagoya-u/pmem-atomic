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

#ifndef PMEM_ATOMIC_UTILITY_HPP
#define PMEM_ATOMIC_UTILITY_HPP

// C++ standard libraries
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace dbgroup::pmem::atomic
{
/*##############################################################################
 * Global enum and constants
 *############################################################################*/

/// @brief Assumes that the length of one word is 8 bytes.
constexpr size_t kWordSize = 8;

/// @brief Assumes that the size of one cache line is 64 bytes.
constexpr size_t kCacheLineSize = 64;

/// @brief Assumes that the size of PMEM read/write units is 256 bytes.
constexpr size_t kPMEMLineSize = 256;

/// @brief A flag for indicating dirty (i.e., unflushed) values.
constexpr uint64_t kDirtyFlag = 0b1UL << 63UL;

/// @brief A flag for indicating PMwCAS descriptors.
constexpr uint64_t kPMwCASFlag = 0b1UL << 62UL;

/// @brief A flag for indicating intermediate states.
constexpr uint64_t kIsIntermediate = kDirtyFlag | kPMwCASFlag;

/*##############################################################################
 * Tuning parameters
 *############################################################################*/

/// @brief The maximum number of target words of PMwCAS.
constexpr size_t kPMwCASCapacity = PMEM_ATOMIC_PMWCAS_CAPACITY;

/// @brief The maximum number of retries for preventing busy loops.
constexpr size_t kRetryNum = PMEM_ATOMIC_SPINLOCK_RETRY_NUM;

/// @brief A back-off time for preventing busy loops [us].
constexpr std::chrono::microseconds kBackOffTime{PMEM_ATOMIC_BACKOFF_TIME};

/*##############################################################################
 * Global utility functions
 *############################################################################*/

/**
 * @tparam T a PMwCAS target class.
 * @retval true if a target class can be updated by PMwCAS.
 * @retval false otherwise.
 */
template <class T>
constexpr auto
CanPCAS()  //
    -> bool
{
  return std::is_same_v<T, uint64_t> || std::is_pointer_v<T>;
}

}  // namespace dbgroup::pmem::atomic

#endif  // PMEM_ATOMIC_UTILITY_HPP
