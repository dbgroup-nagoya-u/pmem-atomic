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

#ifndef PMEM_ATOMIC_COMPONENT_COMMON_HPP
#define PMEM_ATOMIC_COMPONENT_COMMON_HPP

// C++ standard libraries
#include <atomic>
#include <cstring>
#include <type_traits>

// local sources
#include "pmem/atomic/utility.hpp"

namespace dbgroup::pmem::atomic::component
{
/*##############################################################################
 * Global enum and constants
 *############################################################################*/

/**
 * @brief The progress states of PMwCAS operations.
 *
 */
enum DescStatus {
  kCompleted = 0,
  kFailed,
  kSucceeded,
};

/// @brief An alias of std::memory_order_relaxed.
constexpr std::memory_order kMORelax = std::memory_order_relaxed;

/*##############################################################################
 * Internal utilities
 *############################################################################*/

/**
 * @brief Persist a given value if it includes a dirty flag.
 *
 * @param[in] word_addr An address of a target word.
 * @param[in,out] word A word that may be dirty.
 */
void ResolveIntermediateState(  //
    std::atomic_uint64_t *word_addr,
    uint64_t &word);

/**
 * @tparam T A target class.
 * @retval true if a given class is atomically modifiable.
 * @retval false otherwise.
 */
template <class T>
constexpr auto
IsAtomic()  //
    -> bool
{
  return sizeof(T) == kWordSize              //
         && std::is_trivially_copyable_v<T>  //
         && std::is_copy_constructible_v<T>  //
         && std::is_move_constructible_v<T>  //
         && std::is_copy_assignable_v<T>     //
         && std::is_move_assignable_v<T>     //
         && CanPCAS<T>();
}

/**
 * @tparam T A target class.
 * @param obj A source object to be converted.
 * @return A converted unsigned integer.
 */
template <class T>
auto
ToUInt64(   //
    T obj)  //
    -> uint64_t
{
  if constexpr (std::is_pointer_v<T>) {
    return reinterpret_cast<uint64_t>(obj);
  } else {
    uint64_t ret;
    std::memcpy(&ret, static_cast<void *>(&obj), kWordSize);
    return ret;
  }
}

}  // namespace dbgroup::pmem::atomic::component

#endif  // PMEM_ATOMIC_COMPONENT_COMMON_HPP
