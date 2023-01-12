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
#include "component/element_holder.hpp"
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

    // initialize reservation status
    for (size_t i = 0; i < kDescriptorPoolSize; ++i) {
      reserve_arr_[i].store(false);
    }

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
  auto
  Get()  //
      -> PMwCASDescriptor *
  {
    thread_local std::unique_ptr<ElementHolder> ptr = nullptr;

    while (!ptr) {
      for (size_t i = 0; i < kDescriptorPoolSize; ++i) {
        auto is_reserved = reserve_arr_[i].load(std::memory_order_relaxed);
        if (!is_reserved) {
          reserve_arr_[i].compare_exchange_strong(is_reserved, true, std::memory_order_relaxed);
          if (!is_reserved) {
            auto root = pmem_pool_.root();
            ptr = std::make_unique<ElementHolder>(i, reserve_arr_, &(root->descriptors[i]));
            break;
          }
        }
      }
    }

    return ptr->GetHoldDescriptor();
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
    PMwCASDescriptor descriptors[kDescriptorPoolSize]{};
  };

  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  PmemPool_t<DescArray> pmem_pool_;

  /// An array representing the occupied state of the descriptor pool
  std::shared_ptr<std::atomic_bool[]> reserve_arr_{new std::atomic_bool[kDescriptorPoolSize]};
};

}  // namespace dbgroup::atomic::pmwcas

#endif
