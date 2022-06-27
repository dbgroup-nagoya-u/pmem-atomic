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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace dbgroup::atomic::pmwcas
{
/*######################################################################################
 * Global enum and constants
 *####################################################################################*/

#ifdef PMWCAS_CAPACITY
/// The maximum number of target words of PMwCAS
constexpr size_t kPMwCASCapacity = PMWCAS_CAPACITY;
#else
/// The maximum number of target words of PMwCAS
constexpr size_t kPMwCASCapacity = 4;
#endif

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
  if constexpr (std::is_same_v<T, uint64_t> || std::is_pointer_v<T>) {
    return true;
  } else {
    return false;
  }
}

}  // namespace dbgroup::atomic::pmwcas

#endif  // PMWCAS_UTILITY_HPP