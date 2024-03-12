/*
 * Copyright 2024 Database Group, Nagoya University
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

// the corresponding header
#include "pmem/atomic/atomic.hpp"

// C++ standard libraries
#include <atomic>
#include <cstdint>
#include <memory>

// external system libraries
#include <libpmem.h>

// local sources
#include "pmem/atomic/utility.hpp"

namespace dbgroup::pmem::atomic
{

// specialization for unsigned integers.
template <>
auto
PCAS<uint64_t>(  //
    uint64_t *addr,
    uint64_t &expected,
    const uint64_t desired,
    const std::memory_order success,
    const std::memory_order failure)  //
    -> bool
{
  constexpr auto kMORelax = std::memory_order_relaxed;

  const auto orig_expected = expected;
  auto *word_addr = reinterpret_cast<std::atomic_uint64_t *>(addr);
  auto dirty_v = desired | kDirtyFlag;
  while (!word_addr->compare_exchange_weak(expected, dirty_v, kMORelax, failure)) {
    if (expected & kDirtyFlag) {
      component::ResolveIntermediateState(word_addr, expected);
    }
    if (expected != orig_expected) return false;
  }

  pmem_persist(addr, kWordSize);
  word_addr->compare_exchange_strong(dirty_v, desired, success, kMORelax);
  return true;
}

}  // namespace dbgroup::pmem::atomic
