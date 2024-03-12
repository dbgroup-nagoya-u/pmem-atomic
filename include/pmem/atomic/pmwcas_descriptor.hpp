/*
 * Copyright 2021 Database Group, Nagoya University
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

#ifndef PMEM_ATOMIC_PMWCAS_DESCRIPTOR_HPP
#define PMEM_ATOMIC_PMWCAS_DESCRIPTOR_HPP

// C++ standard libraries
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>

// local sources
#include "pmem/atomic/component/common.hpp"
#include "pmem/atomic/component/pmwcas_target.hpp"
#include "pmem/atomic/utility.hpp"

namespace dbgroup::pmem::atomic
{
/**
 * @brief A class to manage a PMwCAS (multi-words compare-and-swap) operation.
 *
 */
class alignas(kPMEMLineSize) PMwCASDescriptor
{
  /*############################################################################
   * Type aliases
   *##########################################################################*/

  using DescStatus = component::DescStatus;
  using PMwCASTarget = component::PMwCASTarget;

 public:
  /*############################################################################
   * Public constructors and assignment operators
   *##########################################################################*/

  PMwCASDescriptor() = delete;

  PMwCASDescriptor(const PMwCASDescriptor &) = delete;
  PMwCASDescriptor(PMwCASDescriptor &&) = delete;

  auto operator=(const PMwCASDescriptor &obj) -> PMwCASDescriptor & = delete;
  auto operator=(PMwCASDescriptor &&) -> PMwCASDescriptor & = delete;

  /*############################################################################
   * Public destructors
   *##########################################################################*/

  /**
   * @brief Destroy the PMwCASDescriptor object.
   *
   */
  ~PMwCASDescriptor() = default;

  /*############################################################################
   * Public getters/setters
   *##########################################################################*/

  /**
   * @return The number of targets added to PMwCAS.
   */
  [[nodiscard]] constexpr auto
  Size() const  //
      -> size_t
  {
    return target_count_;
  }

  /*############################################################################
   * Public utilities
   *##########################################################################*/

  /**
   * @brief Add a new PMwCAS target to this descriptor.
   *
   * @tparam T A class of a target.
   * @param addr A target memory address.
   * @param old_val An expected value of a target field.
   * @param new_val An inserting value into a target field.
   * @param fence A flag for controling std::memory_order.
   */
  template <class T>
  void
  Add(  //
      void *addr,
      const T old_val,
      const T new_val,
      const std::memory_order fence = std::memory_order_seq_cst)
  {
    assert(target_count_ < kPMwCASCapacity);
    targets_[target_count_++] = PMwCASTarget{addr, old_val, new_val, fence};
  }

  /**
   * @brief Perform a PMwCAS operation by using registered targets.
   *
   * @retval true if a PMwCAS operation succeeds.
   * @retval false if a PMwCAS operation fails.
   */
  auto PMwCAS()  //
      -> bool;

  /**
   * @brief Initialize this descriptor and perform recovery if needed.
   *
   */
  void Initialize();

 private:
  /*############################################################################
   * Internal member variables
   *##########################################################################*/

  /// @brief The current state of a PMwCAS operation.
  DescStatus status_{DescStatus::kCompleted};

  /// @brief The number of targets added to PMwCAS.
  size_t target_count_{0};

  /// @brief The address of this descriptor for identification.
  uint64_t desc_addr_{kPMwCASFlag};

  /// @brief Target instances of PMwCAS.
  PMwCASTarget targets_[kPMwCASCapacity];
};

}  // namespace dbgroup::pmem::atomic

#endif  // PMEM_ATOMIC_PMWCAS_DESCRIPTOR_HPP
