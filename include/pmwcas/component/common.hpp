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
#ifdef PMWCAS_HAS_SPINLOCK_HINT
#include <xmmintrin.h>
#define PMWCAS_SPINLOCK_HINT _mm_pause();  // NOLINT
#else
#define PMWCAS_SPINLOCK_HINT /* do nothing */
#endif

// local sources
#include "pmwcas/utility.hpp"

namespace dbgroup::atomic::pmwcas::component
{
/*######################################################################################
 * Global enum and constants
 *####################################################################################*/

/**
 * @brief The progress states of PMwCAS operations.
 *
 */
enum DescStatus {
  kFinished = 0,
  kUndecided,
  kSucceeded,
};

/// @brief An alias of std::memory_order_relaxed.
constexpr std::memory_order kMORelax = std::memory_order_relaxed;

}  // namespace dbgroup::atomic::pmwcas::component

#endif  // PMWCAS_COMPONENT_COMMON_HPP
