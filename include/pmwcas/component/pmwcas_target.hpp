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

// C++ standard libraries
#include <atomic>

// external system libraries
#include <libpmem.h>
#include <libpmemobj.h>

// local sources
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
      : oid_{pmemobj_oid(addr)}, old_val_{old_val}, new_val_{new_val}, fence_{fence}
  {
  }

  constexpr PMwCASTarget(const PMwCASTarget &) = default;
  constexpr PMwCASTarget(PMwCASTarget &&) = default;
  constexpr auto operator=(const PMwCASTarget &obj) -> PMwCASTarget & = default;
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
    auto *addr = static_cast<std::atomic<PMwCASField> *>(pmemobj_direct(oid_));
    PMwCASField expected = old_val_;
    for (size_t i = 0; true; ++i) {
      // try to embed a PMwCAS descriptor
      addr->compare_exchange_strong(expected, desc_addr, std::memory_order_relaxed);
      if (!expected.IsPMwCASDescriptor() || !expected.IsNotPersisted() || i >= kRetryNum) break;

      // retry if another descriptor is embedded
      expected = old_val_;
      SPINLOCK_HINT
    }

    return expected == old_val_;
  }

  /**
   * @brief Flush a value of this target address.
   *
   */
  void
  Flush()
  {
    auto *addr = static_cast<std::atomic<PMwCASField> *>(pmemobj_direct(oid_));
    pmem_flush(addr, kWordSize);
  }

  /**
   * @brief Update a value of this target address.
   *
   */
  void
  RedoPMwCAS()
  {
    auto *addr = static_cast<std::atomic<PMwCASField> *>(pmemobj_direct(oid_));
    addr->store(new_val_.GetCopyWithDirtyFlag(), std::memory_order_relaxed);
    pmem_persist(addr, kWordSize);
    addr->store(new_val_, fence_);
    pmem_persist(addr, kWordSize);
  }

  /**
   * @brief Revert a value of this target address.
   *
   */
  void
  UndoPMwCAS()
  {
    auto *addr = static_cast<std::atomic<PMwCASField> *>(pmemobj_direct(oid_));
    addr->store(old_val_.GetCopyWithDirtyFlag(), std::memory_order_relaxed);
    pmem_persist(addr, kWordSize);
    addr->store(old_val_, std::memory_order_relaxed);
    pmem_persist(addr, kWordSize);
  }

  /**
   * @brief Recover and flush the target.
   *
   */
  void
  Recover(  //
      const bool succeeded,
      PMwCASField desc_addr)
  {
    auto *addr = static_cast<std::atomic<PMwCASField> *>(pmemobj_direct(oid_));
    const auto desired = (succeeded) ? new_val_ : old_val_;
    addr->compare_exchange_strong(desc_addr, desired, std::memory_order_relaxed);

    // if CAS failed, `desc_addr` has the current value
    if (desc_addr.IsNotPersisted()) {
      const auto v = desc_addr.GetCopyWithoutDirtyFlag();
      addr->store(v, std::memory_order_relaxed);
    }
    pmem_flush(addr, kWordSize);
  }

 private:
  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// A target memory address
  PMEMoid oid_{};

  /// An expected value of a target field
  PMwCASField old_val_{};

  /// An inserting value into a target field
  PMwCASField new_val_{};

  /// A fence to be inserted when embedding a new value.
  std::memory_order fence_{std::memory_order_seq_cst};
};

}  // namespace dbgroup::atomic::pmwcas::component

#endif  // PMWCAS_COMPONENT_PMWCAS_TARGET_HPP
