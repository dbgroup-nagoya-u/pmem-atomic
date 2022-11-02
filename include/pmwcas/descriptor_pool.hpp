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
#include <iostream>
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/make_persistent_atomic.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <memory>
#include <utility>

#include "element_holder.hpp"
#include "pmwcas_descriptor.hpp"

namespace dbgroup::atomic::pmwcas
{

class DescriptorPool
{
  /*################################################################################################
   * Type aliases
   *##############################################################################################*/

  template <class T>
  using PmemPool = ::pmem::obj::pool<T>;

  template <class T>
  using PmemPtr = ::pmem::obj::persistent_ptr<T>;

 public:
  /*####################################################################################
   * Public constructors and assignment operators
   *##################################################################################*/
  DescriptorPool(const std::string &path)
      : pop_{PmemPool<DescPool>::create(
          path, "desc_pool", sizeof(DescPool) * 2, S_IRUSR | S_IWUSR)},
        root_{pop_.root()}
  {
  }

  /*################################################################################################
   * Public destructors
   *##############################################################################################*/

  ~DescriptorPool() { pop_.close(); }

  /*####################################################################################
   * Public utility functions
   *##################################################################################*/
  auto
  Get()  //
      -> PMwCASDescriptor *
  {
    thread_local std::unique_ptr<ElementHolder> ptr = nullptr;

    while (!ptr) {
      for (auto &element : root_->pool) {
        auto is_reserved = element.get_ro().is_reserved.load(std::memory_order_relaxed);
        if (!is_reserved) {
          auto is_changed = element.get_rw().is_reserved.compare_exchange_strong(
              is_reserved, true, std::memory_order_relaxed);
          if (is_changed) {
            ptr = std::make_unique<ElementHolder>(&element);
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

  struct DescPool {
    ::pmem::obj::p<PoolElement> pool[kDescriptorPoolSize];
  };

  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// Descriptor pool
  // PoolElement pool_[kDescriptorPoolSize];

  /// pool object pointer
  PmemPool<DescPool> pop_;

  /// a pointer to the root object
  PmemPtr<DescPool> root_;
};

}  // namespace dbgroup::atomic::pmwcas

#endif
