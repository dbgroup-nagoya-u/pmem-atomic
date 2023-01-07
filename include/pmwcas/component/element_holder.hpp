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

// C++ standard libraries
#include <atomic>

// local sources
#include "pmwcas_descriptor.hpp"

namespace dbgroup::atomic::pmwcas
{

class ElementHolder
{
 public:
  /*####################################################################################
   * Public constructors and assignment operators
   *##################################################################################*/

  ElementHolder(  //
      const size_t pos,
      std::shared_ptr<std::atomic_bool[]> reserved_arr,
      PMwCASDescriptor *desc)
      : pos_{pos}, desc_{desc}, reserved_arr_{std::move(reserved_arr)}
  {
  }

  ElementHolder(const ElementHolder &) = delete;
  ElementHolder(ElementHolder &&) = delete;
  auto operator=(const ElementHolder &) -> ElementHolder & = delete;
  auto operator=(ElementHolder &&) -> ElementHolder & = delete;

  /*####################################################################################
   * Public destructors
   *##################################################################################*/

  ~ElementHolder() { reserved_arr_[pos_].store(false, std::memory_order_relaxed); }

  /*####################################################################################
   * Public getters
   *##################################################################################*/

  [[nodiscard]] constexpr auto
  GetHoldDescriptor() const  //
      -> PMwCASDescriptor *
  {
    return desc_;
  }

 private:
  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// the position of a descriptor in a pool.
  size_t pos_{0};

  /// an address of a descriptor.
  PMwCASDescriptor *desc_{nullptr};

  /// an actual reservation flag for this object.
  std::shared_ptr<std::atomic_bool[]> reserved_arr_{nullptr};
};

}  // namespace dbgroup::atomic::pmwcas
