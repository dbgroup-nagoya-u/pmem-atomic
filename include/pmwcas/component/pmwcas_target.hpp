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
#include "pmwcas_word.hpp"

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
   * @tparam T A class of PMwCAS targets.
   * @param addr A target memory address.
   * @param old_val An expected value of the target address.
   * @param new_val An desired value of the target address.
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
   * @param desc_addr A memory address of a target descriptor.
   * @retval true if the descriptor address is successfully embedded.
   * @retval false otherwise.
   */
  auto
  EmbedDescriptor(const PMwCASWord desc_addr)  //
      -> bool
  {
    auto *addr = static_cast<std::atomic<PMwCASWord> *>(pmemobj_direct(oid_));
    PMwCASWord expected{};
    for (size_t i = 0; true; ++i) {
      // try to embed a PMwCAS descriptor
      expected = addr->load(std::memory_order_relaxed);
      if (expected == old_val_
          && addr->compare_exchange_strong(expected, desc_addr, std::memory_order_relaxed)) {
        return true;
      }
      if (!expected.IsIntermediate() || i >= kRetryNum) return false;

      // retry if another descriptor is embedded
      PMWCAS_SPINLOCK_HINT
    }
  }

  /**
   * @brief Flush a value of this target address.
   *
   */
  void
  Flush()
  {
    auto *addr = static_cast<std::atomic<PMwCASWord> *>(pmemobj_direct(oid_));
    pmem_flush(addr, kWordSize);
  }

  /**
   * @brief Update a value of this target address.
   *
   */
  void
  Redo()
  {
    auto *addr = static_cast<std::atomic<PMwCASWord> *>(pmemobj_direct(oid_));

    if constexpr (kUseDirtyFlag) {
      auto dirty_val = new_val_;
      dirty_val.SetDirtyFlag();
      addr->store(dirty_val, std::memory_order_relaxed);
      pmem_persist(addr, kWordSize);
      addr->store(new_val_, fence_);
      pmem_persist(addr, kWordSize);
    } else {
      addr->store(new_val_, fence_);
      pmem_flush(addr, kWordSize);
    }
  }

  /**
   * @brief Revert a value of this target address.
   *
   */
  void
  Undo()
  {
    auto *addr = static_cast<std::atomic<PMwCASWord> *>(pmemobj_direct(oid_));

    if constexpr (kUseDirtyFlag) {
      auto dirty_val = old_val_;
      dirty_val.SetDirtyFlag();
      addr->store(dirty_val, std::memory_order_relaxed);
      pmem_persist(addr, kWordSize);
      addr->store(old_val_, std::memory_order_relaxed);
      pmem_persist(addr, kWordSize);
    } else {
      addr->store(old_val_, std::memory_order_relaxed);
      pmem_flush(addr, kWordSize);
    }
  }

  /**
   * @brief Recover and flush the target.
   *
   */
  void
  Recover(  //
      const bool succeeded,
      PMwCASWord desc_addr)
  {
    auto *addr = static_cast<std::atomic<PMwCASWord> *>(pmemobj_direct(oid_));
    const auto desired = (succeeded) ? new_val_ : old_val_;
    addr->compare_exchange_strong(desc_addr, desired, std::memory_order_relaxed);

    if constexpr (kUseDirtyFlag) {
      // if CAS failed, `desc_addr` has the current value
      if (desc_addr.IsNotPersisted()) {
        auto val = desc_addr;
        val.ClearDirtyFlag();
        addr->store(val, std::memory_order_relaxed);
      }
    }

    pmem_flush(addr, kWordSize);
  }

 private:
  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// @brief A target memory address.
  PMEMoid oid_{};

  /// @brief An expected value of a target field.
  PMwCASWord old_val_{};

  /// @brief An inserting value into a target field.
  PMwCASWord new_val_{};

  /// @brief A fence to be inserted when embedding a new value.
  std::memory_order fence_{std::memory_order_seq_cst};
};

}  // namespace dbgroup::atomic::pmwcas::component

#endif  // PMWCAS_COMPONENT_PMWCAS_TARGET_HPP
