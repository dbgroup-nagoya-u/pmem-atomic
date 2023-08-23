/*
 * Copyright 2022 Database Group, Nagoya University
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

#ifndef PMWCAS_DESCRIPTOR_POOL_HPP
#define PMWCAS_DESCRIPTOR_POOL_HPP

// system headers
#include <sys/stat.h>

// C++ standard libraries
#include <atomic>
#include <filesystem>
#include <iostream>
#include <memory>
#include <utility>

// local sources
#include "component/pmwcas_descriptor.hpp"

namespace dbgroup::atomic::pmwcas
{

class DescriptorPool
{
  /*################################################################################################
   * Type aliases
   *##############################################################################################*/

  using DescStatus = component::DescStatus;

 public:
  /*####################################################################################
   * Public constructors and assignment operators
   *##################################################################################*/

  /**
   * @brief Construct a new DescriptorPool object.
   *
   * @param pmem_path the path to a pmemobj pool for PMwCAS.
   * @param layout_name the layout name to distinguish application.
   */
  explicit DescriptorPool(  //
      const std::string &pmem_path,
      const std::string &layout_name = "pmwcas_desc_pool")
  {
    constexpr size_t kSize = sizeof(PMwCASDescriptor) * (kMaxThreadNum + 1) + PMEMOBJ_MIN_POOL;
    constexpr auto kModeRW = S_IRUSR | S_IWUSR;  // NOLINT
    constexpr uintptr_t kMask = kPMEMLineSize - 1;

    // create/open a pool on persistent memory
    const auto *path = pmem_path.c_str();
    const auto *layout = layout_name.c_str();
    const auto may_dirty = std::filesystem::exists(pmem_path);
    if (may_dirty) {
      pop_ = pmemobj_open(path, layout);
    } else {
      pop_ = pmemobj_create(path, layout, kSize, kModeRW);
    }
    if (pop_ == nullptr) {
      std::cerr << pmemobj_errormsg() << std::endl;
      throw std::exception{};
    }

    // get the pointer to descriptors
    auto &&root = pmemobj_root(pop_, kSize);
    auto addr = reinterpret_cast<uintptr_t>(pmemobj_direct(root));
    if ((addr & kMask) > 0) {
      // move the address to the Intel Optane alignment
      addr = (addr & ~kMask) + kPMEMLineSize;
    }
    desc_pool_ = reinterpret_cast<PMwCASDescriptor *>(addr);

    // if the pool was on persistent memory, check for dirty descriptors
    if (may_dirty) {
      for (size_t i = 0; i < kMaxThreadNum; ++i) {
        desc_pool_[i].CompletePMwCAS();
      }
    }
  }

  DescriptorPool(const DescriptorPool &) = delete;
  DescriptorPool(DescriptorPool &&) = delete;
  auto operator=(const DescriptorPool &) -> DescriptorPool & = delete;
  auto operator=(DescriptorPool &&) -> DescriptorPool & = delete;

  /*################################################################################################
   * Public destructors
   *##############################################################################################*/

  /**
   * @brief Destroy the DescriptorPool object.
   *
   */
  ~DescriptorPool()
  {
    if (pop_ != nullptr) {
      pmemobj_close(pop_);
    }
  }

  /*####################################################################################
   * Public utility functions
   *##################################################################################*/

  /**
   * @note If a thread calls this function multiple times, it will return the same
   * descriptor.
   * @return The PMwCAS descriptor for the current thread.
   */
  auto
  Get()  //
      -> PMwCASDescriptor *
  {
    return &(desc_pool_[::dbgroup::thread::IDManager::GetThreadID()]);
  }

 private:
  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// @brief The pmemobj_pool for holding PMwCAS descriptors.
  PMEMobjpool *pop_ = nullptr;

  /// @brief The pool of PMwCAS descriptors.
  PMwCASDescriptor *desc_pool_ = nullptr;
};

}  // namespace dbgroup::atomic::pmwcas

#endif
