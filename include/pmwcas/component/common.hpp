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

#ifndef PMWCAS_COMPONENT_COMMON_HPP
#define PMWCAS_COMPONENT_COMMON_HPP

// C++ standard libraries
#include <atomic>

// external system libraries
#ifndef SPINLOCK_HINT
#ifdef PMWCAS_HAS_SPINLOCK_HINT
#include <xmmintrin.h>
#define SPINLOCK_HINT _mm_pause();  // NOLINT
#else
#define SPINLOCK_HINT /* do nothing */
#endif
#endif

// local sources
#include "pmwcas/utility.hpp"

namespace dbgroup::atomic::pmwcas::component
{
/*######################################################################################
 * Global enum and constants
 *####################################################################################*/

enum DescStatus {
  kFinished = 0,
  kUndecided,
  kSucceeded,
};

/*######################################################################################
 * Global utility structs
 *####################################################################################*/

/**
 * @brief An union to convert PMwCAS target data into uint64_t.
 *
 * @tparam T a type of target data
 */
template <class T>
union CASTargetConverter {
  const T target_data;
  const uint64_t converted_data;

  explicit constexpr CASTargetConverter(const uint64_t converted) : converted_data{converted} {}

  explicit constexpr CASTargetConverter(const T target) : target_data{target} {}
};

/**
 * @brief Specialization for unsigned long type.
 *
 */
template <>
union CASTargetConverter<uint64_t> {
  const uint64_t target_data;
  const uint64_t converted_data;

  explicit constexpr CASTargetConverter(const uint64_t target) : target_data{target} {}
};

}  // namespace dbgroup::atomic::pmwcas::component

#endif  // PMWCAS_COMPONENT_COMMON_HPP
