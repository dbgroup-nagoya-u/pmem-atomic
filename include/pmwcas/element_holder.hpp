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

#include <atomic>

#include "pmwcas_descriptor.hpp"

namespace dbgroup::atomic::pmwcas
{
struct PoolElement {
 public:
  /// a flag for indicating this element is reserved.
  std::atomic_bool is_reserved{false};

  /// a target instance.
  PMwCASDescriptor desc{};
};

class ElementHolder
{
 public:
  /*####################################################################################
   * Public constructors and assignment operators
   *##################################################################################*/

  explicit ElementHolder(PoolElement *element) : element_{element} {}

  ElementHolder(const ElementHolder &) = delete;
  ElementHolder(ElementHolder &&) = delete;
  auto operator=(const ElementHolder &) -> ElementHolder & = delete;
  auto operator=(ElementHolder &&) -> ElementHolder & = delete;

  /*####################################################################################
   * Public destructors
   *##################################################################################*/

  ~ElementHolder() { element_->is_reserved.store(false, std::memory_order_relaxed); }

  /*####################################################################################
   * Public getters
   *##################################################################################*/

  constexpr auto
  GetHoldDescriptor() const  //
      -> PMwCASDescriptor *
  {
    return &(element_->desc);
  }

 private:
  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// an address of an actual element.
  PoolElement *element_{};
};

}  // namespace dbgroup::atomic::pmwcas
