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
#include <memory>
#include <utility>

#include "element_holder.hpp"
#include "pmwcas_descriptor.hpp"

namespace dbgroup::atomic::pmwcas
{

class DescriptorPool
{
 public:
  /*####################################################################################
   * Public constructors and assignment operators
   *##################################################################################*/
  DescriptorPool() {}

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
            ptr = std::make_unique<ElementHolder>(i, reserve_arr_, pool_);
            break;
          }
        }
      }
    }

    return ptr->GetHoldDescriptor();
  }

 private:
  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// Descriptor pool
  PMwCASDescriptor pool_[kDescriptorPoolSize];

  std::shared_ptr<std::atomic_bool[]> reserve_arr_{new std::atomic_bool[kDescriptorPoolSize]};
};

}  // namespace dbgroup::atomic::pmwcas

#endif
