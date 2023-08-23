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

// C++ standard libraries
#include <atomic>
#include <filesystem>
#include <iostream>
#include <memory>
#include <utility>

// external system libraries
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>

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
  template <class T>
  using PmemPool_t = ::pmem::obj::pool<T>;

 public:
  /*####################################################################################
   * Public constructors and assignment operators
   *##################################################################################*/

  DescriptorPool(  //
      const std::string &path,
      const std::string &layout)
  {
    constexpr auto kModeRW = S_IRUSR | S_IWUSR;  // NOLINT

    // create/open a pool on persistent memory
    try {
      if (std::filesystem::exists(path)) {
        pmem_pool_ = PmemPool_t<DescArray>::open(path, layout);
        Recovery();
      } else {
        constexpr size_t kSize = ((sizeof(DescArray) / PMEMOBJ_MIN_POOL) + 2) * PMEMOBJ_MIN_POOL;
        pmem_pool_ = PmemPool_t<DescArray>::create(path, layout, kSize, kModeRW);
      }
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
      std::terminate();
    }
  }

  DescriptorPool(const DescriptorPool &) = delete;
  DescriptorPool(DescriptorPool &&) = delete;
  auto operator=(const DescriptorPool &) -> DescriptorPool & = delete;
  auto operator=(DescriptorPool &&) -> DescriptorPool & = delete;

  /*################################################################################################
   * Public destructors
   *##############################################################################################*/

  ~DescriptorPool()
  {
    try {
      pmem_pool_.close();
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
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

  void
  Recovery()
  {
    auto root = pmem_pool_.root();

    for (size_t i = 0; i < kDescriptorPoolSize; ++i) {
      auto desc = root->descriptors[i];
      desc.CompletePMwCAS();
    }
  }

 private:
  /*################################################################################################
   * Internal classes/structs
   *##############################################################################################*/

  /// Descriptor pool array
  struct DescArray {
    PMwCASDescriptor descriptors[kMaxThreadNum]{};
  };

  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// @brief The pool of PMwCAS descriptors.
  PMwCASDescriptor *desc_pool_ = nullptr;

  PmemPool_t<DescArray> pmem_pool_;
};

}  // namespace dbgroup::atomic::pmwcas

#endif
