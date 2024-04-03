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
#include "pmem/atomic/pmwcas_descriptor.hpp"

// C++ standard libraries
#include <cstddef>

// external system libraries
#include <libpmem.h>

// local sources
#include "pmem/atomic/utility.hpp"

namespace dbgroup::pmem::atomic
{
namespace
{
/*##############################################################################
 * Local constants
 *############################################################################*/

/// @brief The size of a PMwCAS header in bytes.
constexpr size_t kHeaderSize = 2 * kWordSize;

}  // namespace

/*##############################################################################
 * Public utilities
 *############################################################################*/

auto
PMwCASDescriptor::PMwCAS()  //
    -> bool
{
  const size_t desc_size = kHeaderSize + kWordSize + sizeof(PMwCASTarget) * target_count_;

  // initialize and persist PMwCAS status
  status_ = DescStatus::kFailed;
  pmem_persist(this, desc_size);

  // linearize PMwCAS operations by embedding a descriptor
  size_t embedded_count = 0;
  for (size_t i = 0;                                                  //
       i < target_count_ && targets_[i].EmbedDescriptor(desc_addr_);  //
       ++i, ++embedded_count) {
  }

  // complete PMwCAS
  if (embedded_count < target_count_) {
    // PMwCAS failed, so revert changes
    for (size_t i = 0; i < embedded_count; ++i) {
      targets_[i].Undo();
    }
    pmem_drain();

    // reset the descriptor
    status_ = DescStatus::kCompleted;
    target_count_ = 0;
    return false;
  }

  // PMwCAS succeeded, so persist the embedded descriptors for fault-tolerance
  for (size_t i = 0; i < target_count_; ++i) {
    targets_[i].Flush();
  }
  status_ = DescStatus::kSucceeded;
  pmem_flush(this, kHeaderSize);
  pmem_drain();

  // update the target address with the desired values
  for (size_t i = 0; i < target_count_; ++i) {
    targets_[i].Redo();
  }
  pmem_drain();

  // reset the descriptor
  status_ = DescStatus::kCompleted;
  target_count_ = 0;
  return true;
}

void
PMwCASDescriptor::Initialize()
{
  desc_addr_ = pmemobj_oid(this).off | kPMwCASFlag;

  // roll forward or roll back PMwCAS if needed
  if (status_ != DescStatus::kCompleted) {
    const auto succeeded = (status_ == DescStatus::kSucceeded);
    for (size_t i = 0; i < target_count_; ++i) {
      targets_[i].Recover(succeeded, desc_addr_);
    }
  }

  // change status and persist a descriptor
  status_ = DescStatus::kCompleted;
  target_count_ = 0;
  pmem_flush(this, kHeaderSize + kWordSize);
}

}  // namespace dbgroup::pmem::atomic
