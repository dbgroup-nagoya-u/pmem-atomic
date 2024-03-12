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
#include "pmem/atomic/component/pmwcas_target.hpp"

// C++ standard libraries
#include <atomic>
#include <cstddef>
#include <cstdint>

// external system libraries
#include <libpmem.h>
#include <libpmemobj.h>

// external libraries
#include "lock/common.hpp"

// local sources
#include "pmem/atomic/utility.hpp"

namespace dbgroup::pmem::atomic::component
{

auto
PMwCASTarget::EmbedDescriptor(  //
    const uint64_t desc_addr)   //
    -> bool
{
  auto *addr = static_cast<std::atomic_uint64_t *>(pmemobj_direct(oid_));
  for (size_t i = 0; true; ++i) {
    auto expected = addr->load(kMORelax);
    if (expected == old_val_
        && addr->compare_exchange_strong(expected, desc_addr, fence_, kMORelax)) {
      return true;
    }
    if ((expected & kIsIntermediate) == 0 || i >= kRetryNum) return false;
    CPP_UTILITY_SPINLOCK_HINT
  }
}

void
PMwCASTarget::Flush()
{
  auto *addr = static_cast<std::atomic_uint64_t *>(pmemobj_direct(oid_));
  pmem_flush(addr, kWordSize);
}
void
PMwCASTarget::Redo()
{
  auto *addr = static_cast<std::atomic_uint64_t *>(pmemobj_direct(oid_));
  addr->store(new_val_, kMORelax);
  pmem_flush(addr, kWordSize);
}

void
PMwCASTarget::Undo()
{
  auto *addr = static_cast<std::atomic_uint64_t *>(pmemobj_direct(oid_));
  addr->store(old_val_, kMORelax);
  pmem_flush(addr, kWordSize);
}

void
PMwCASTarget::Recover(  //
    const bool succeeded,
    uint64_t desc_addr)
{
  auto *addr = static_cast<std::atomic_uint64_t *>(pmemobj_direct(oid_));
  const auto word = addr->load(kMORelax);
  if (word & kDirtyFlag) {
    addr->store(word & ~kDirtyFlag, kMORelax);
    pmem_flush(addr, kWordSize);
  } else if (word == desc_addr) {
    addr->store(succeeded ? new_val_ : old_val_, kMORelax);
    pmem_flush(addr, kWordSize);
  }
}

}  // namespace dbgroup::pmem::atomic::component
