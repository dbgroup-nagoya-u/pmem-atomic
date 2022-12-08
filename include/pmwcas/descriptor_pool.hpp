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

#include <atomic>
#include <filesystem>
#include <iostream>
#include <memory>
#include <utility>

// libraries for managing persistem memory
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/make_persistent_atomic.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>

#include "component/element_holder.hpp"
#include "component/pmwcas_descriptor.hpp"

namespace dbgroup::atomic::pmwcas
{

class DescriptorPool
{
  /*################################################################################################
   * Type aliases
   *##############################################################################################*/

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
    // initialize reservation status
    for (size_t i = 0; i < kDescriptorPoolSize; ++i) {
      reserve_arr_[i].store(false);
    }

    // create/open a pool on persistent memory
    try {
      if (std::filesystem::exists(path)) {
        pmem_pool_ = PmemPool_t<DescArray>::open(path, layout);
        // 途中状態のrecovery
      } else {
        constexpr size_t kSize = ((sizeof(DescArray) / PMEMOBJ_MIN_POOL) + 2) * PMEMOBJ_MIN_POOL;
        pmem_pool_ = PmemPool_t<DescArray>::create(path, layout, kSize, S_IRUSR | S_IWUSR);
      }
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  /*################################################################################################
   * Public destructors
   *##############################################################################################*/

  ~DescriptorPool() { pmem_pool_.close(); }

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
          auto is_changed =
              reserve_arr_[i].compare_exchange_strong(is_reserved, true, std::memory_order_relaxed);
          if (is_changed) {
            auto root = pmem_pool_.root();
            ptr =
                std::make_unique<ElementHolder>(i, reserve_arr_, &(root->descriptors[i].get_rw()));
            break;
          }
        }
      }
    }

    return ptr->GetHoldDescriptor();
  }

 private:
  /*################################################################################################
   * Internal classes/structs
   *##############################################################################################*/

  /// Descriptor pool array
  struct DescArray {
    ::pmem::obj::p<PMwCASDescriptor> descriptors[kDescriptorPoolSize]{};
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
