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

#ifndef PMWCAS_COMPONENT_PMWCAS_TARGET_HPP
#define PMWCAS_COMPONENT_PMWCAS_TARGET_HPP

#include <atomic>

#include "pmwcas_field.hpp"

namespace dbgroup::atomic::pmwcas::component
{
/**
 * @brief A class to represent a PMwCAS target.
 *
 */
class PMwCASTarget
{
 public:
  /*####################################################################################
   * Public constructors and assignment operators
   *##################################################################################*/

  /**
   * @brief Construct an empty PMwCAS target.
   *
   */
  constexpr PMwCASTarget() = default;

  /**
   * @brief Construct a new PMwCAS target based on given information.
   *
   * @tparam T a class of PMwCAS targets.
   * @param addr a target memory address.
   * @param old_val an expected value of the target address.
   * @param new_val an desired value of the target address.
   */
  template <class T>
  constexpr PMwCASTarget(  //
      void *addr,
      const T old_val,
      const T new_val,
      const std::memory_order fence)
      : addr_{static_cast<std::atomic<PMwCASField> *>(addr)},
        old_val_{old_val},
        new_val_{new_val},
        fence_{fence}
  {
  }

  constexpr PMwCASTarget(const PMwCASTarget &) = default;
  constexpr auto operator=(const PMwCASTarget &obj) -> PMwCASTarget & = default;
  constexpr PMwCASTarget(PMwCASTarget &&) = default;
  constexpr auto operator=(PMwCASTarget &&) -> PMwCASTarget & = default;

  /*####################################################################################
   * Public destructor
   *##################################################################################*/

  /**
   * @brief Destroy the PMwCASTarget object.
   *
   */
  ~PMwCASTarget() = default;

  /*####################################################################################
   * Public utility functions
   *##################################################################################*/

  /**
   * @brief Embed a descriptor into this target address to linearlize PMwCAS operations.
   *
   * @param desc_addr a memory address of a target descriptor.
   * @retval true if the descriptor address is successfully embedded.
   * @retval false otherwise.
   */
  auto
  EmbedDescriptor(const PMwCASField desc_addr)  //
      -> bool
  {
    PMwCASField expected = old_val_;
    for (size_t i = 0; true; ++i) {
      // try to embed a PMwCAS descriptor
      addr_->compare_exchange_strong(expected, desc_addr, std::memory_order_relaxed);
      if (!expected.IsPMwCASDescriptor() || !expected.IsNotPersisted() || i >= kRetryNum) break;

      // retry if another descriptor is embedded
      expected = old_val_;
      SPINLOCK_HINT
    }

    return expected == old_val_;
  }

  /**
   * @brief Update/revert a value of this target address.
   *
   */
  void
  RedoPMwCAS()
  {
    addr_->store(new_val_.GetCopyWithDirtyFlag(), std::memory_order_relaxed);
    addr_->store(new_val_, fence_);
  }

  /**
   * @brief Revert a value of this target address.
   *
   */
  void
  UndoPMwCAS()
  {
    addr_->store(old_val_.GetCopyWithDirtyFlag(), std::memory_order_relaxed);
    addr_->store(old_val_, std::memory_order_relaxed);
  }

 private:
  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// A target memory address
  std::atomic<PMwCASField> *addr_{};

  /// An expected value of a target field
  PMwCASField old_val_{};

  /// An inserting value into a target field
  PMwCASField new_val_{};

  /// A fence to be inserted when embedding a new value.
  std::memory_order fence_{std::memory_order_seq_cst};
};

}  // namespace dbgroup::atomic::pmwcas::component

#endif  // PMWCAS_COMPONENT_PMWCAS_TARGET_HPP
