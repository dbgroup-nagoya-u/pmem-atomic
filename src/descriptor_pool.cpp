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
#include "pmem/atomic/descriptor_pool.hpp"

// C++ standard libraries
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

// system libraries
#include <sys/stat.h>
#include <sys/types.h>

// external system libraries
#include <libpmemobj.h>

// external libraries
#include "thread/common.hpp"
#include "thread/id_manager.hpp"

// local sources
#include "pmem/atomic/pmwcas_descriptor.hpp"

namespace dbgroup::pmem::atomic
{
/*##############################################################################
 * Public constructors and descructors
 *############################################################################*/

DescriptorPool::DescriptorPool(  //
    const std::string &pmem_path,
    const std::string &layout_name)
{
  constexpr size_t kMaxThreadNum = ::dbgroup::thread::kMaxThreadNum;
  constexpr size_t kDescPoolSize = sizeof(PMwCASDescriptor) * (kMaxThreadNum + 1);
  constexpr size_t kPoolSize = kDescPoolSize + PMEMOBJ_MIN_POOL;
  constexpr mode_t kModeRW = S_IRUSR | S_IWUSR;
  constexpr uintptr_t kMask = kPMEMLineSize - 1;

  // create/open a pool on persistent memory
  const auto *path = pmem_path.c_str();
  const auto *layout = layout_name.c_str();
  pop_ = std::filesystem::exists(pmem_path) ? pmemobj_open(path, layout)
                                            : pmemobj_create(path, layout, kPoolSize, kModeRW);
  if (pop_ == nullptr) throw std::runtime_error{pmemobj_errormsg()};

  // get the pointer to descriptors
  auto &&root = pmemobj_root(pop_, kDescPoolSize);
  auto addr = reinterpret_cast<uintptr_t>(pmemobj_direct(root));
  if ((addr & kMask) > 0) {
    // move the address to the Intel Optane alignment
    addr = (addr & ~kMask) + kPMEMLineSize;
  }
  desc_pool_ = reinterpret_cast<PMwCASDescriptor *>(addr);

  // if the pool was on persistent memory, check for dirty descriptors
  for (size_t i = 0; i < kMaxThreadNum; ++i) {
    desc_pool_[i].Initialize();
  }
  pmem_drain();
}

DescriptorPool::~DescriptorPool()
{
  if (pop_ != nullptr) {
    pmemobj_close(pop_);
  }
}

/*##############################################################################
 * Public utility functions
 *############################################################################*/

auto
DescriptorPool::Get()  //
    -> PMwCASDescriptor *
{
  return &(desc_pool_[::dbgroup::thread::IDManager::GetThreadID()]);
}

}  // namespace dbgroup::pmem::atomic
