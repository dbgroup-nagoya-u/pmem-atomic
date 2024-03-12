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
#include "pmem/atomic/component/common.hpp"

// C++ standard libraries
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>

// external system libraries
#include <libpmem.h>

// external libraries
#include "lock/common.hpp"

// local sources
#include "pmem/atomic/utility.hpp"

namespace dbgroup::pmem::atomic::component
{

void
ResolveIntermediateState(  //
    std::atomic_uint64_t *word_addr,
    uint64_t &word)
{
  while (word & kIsIntermediate) {
    for (size_t i = 0; i < kRetryNum; ++i) {
      CPP_UTILITY_SPINLOCK_HINT
      word = word_addr->load(kMORelax);
      if ((word & kIsIntermediate) == 0) return;
    }

    const auto orig_word = word;
    std::this_thread::sleep_for(kBackOffTime);
    word = word_addr->load(kMORelax);
    if ((word & kIsIntermediate) == 0) return;
    if ((word & kPMwCASFlag) || word != orig_word) continue;

    pmem_persist(word_addr, kWordSize);
    if (word_addr->compare_exchange_strong(word, word & ~kDirtyFlag, kMORelax, kMORelax)) {
      word &= ~kDirtyFlag;
      return;
    }
  }
}

template <>
constexpr auto
ToUInt64<uint64_t>(  //
    uint64_t obj)    //
    -> uint64_t
{
  return obj;
}

}  // namespace dbgroup::pmem::atomic::component
