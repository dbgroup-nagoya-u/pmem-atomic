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

#ifndef PMWCAS_PMWCAS_DESCRIPTOR_HPP
#define PMWCAS_PMWCAS_DESCRIPTOR_HPP

// C++ standard libraries
#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <utility>

// external system libraries
#include <libpmem.h>

// local sources
#include "pmwcas_target.hpp"

namespace dbgroup::atomic::pmwcas
{
/**
 * @brief A class to manage a PMwCAS (multi-words compare-and-swap) operation.
 *
 */
class alignas(component::kCacheLineSize) PMwCASDescriptor
{
  /*####################################################################################
   * Type aliases
   *##################################################################################*/

  using PMwCASTarget = component::PMwCASTarget;
  using PMwCASField = component::PMwCASField;
  using DescStatus = component::DescStatus;

 public:
  /*####################################################################################
   * Public constructors and assignment operators
   *##################################################################################*/

  /**
   * @brief Construct an empty descriptor for PMwCAS operations.
   *
   */
  constexpr PMwCASDescriptor() = default;

  constexpr PMwCASDescriptor(const PMwCASDescriptor &) = default;
  constexpr PMwCASDescriptor(PMwCASDescriptor &&) = default;
  constexpr auto operator=(const PMwCASDescriptor &obj) -> PMwCASDescriptor & = default;
  constexpr auto operator=(PMwCASDescriptor &&) -> PMwCASDescriptor & = default;

  /*####################################################################################
   * Public destructors
   *##################################################################################*/

  /**
   * @brief Destroy the PMwCASDescriptor object.
   *
   */
  ~PMwCASDescriptor() = default;

  /*####################################################################################
   * Public getters/setters
   *##################################################################################*/

  /**
   * @return the number of registered PMwCAS targets
   */
  [[nodiscard]] constexpr auto
  Size() const  //
      -> size_t
  {
    return target_count_;
  }

  /**
   * @brief Reset the current number of targets to zero.
   *
   */
  void
  Reset()
  {
    target_count_ = 0;
    pmem_persist(&target_count_, sizeof(size_t));
  }

  /*####################################################################################
   * Public utility functions
   *##################################################################################*/

  /**
   * @brief Read a value from a given memory address.
   * \e NOTE: if a memory address is included in PMwCAS target fields, it must be read
   * via this function.
   *
   * @tparam T an expected class of a target field
   * @param addr a target memory address to read
   * @param fence a flag for controling std::memory_order.
   * @return a read value
   */
  template <class T>
  static auto
  Read(  //
      const void *addr,
      const std::memory_order fence = std::memory_order_seq_cst)  //
      -> T
  {
    const auto *target_addr = static_cast<const std::atomic<PMwCASField> *>(addr);

    PMwCASField target_word{};
    while (true) {
      for (size_t i = 1; true; ++i) {
        target_word = target_addr->load(fence);

        if constexpr (component::kIsDirtyFlagEnabled) {
          if (!target_word.IsPMwCASDescriptor() || !target_word.IsNotPersisted()) {
            return target_word.GetTargetData<T>();
          }
        } else {
          if (!target_word.IsPMwCASDescriptor()) return target_word.GetTargetData<T>();
        }

        if (i > kRetryNum) break;
        SPINLOCK_HINT
      }

      // wait to prevent busy loop
      std::this_thread::sleep_for(kShortSleep);
    }
  }

  /**
   * @brief Add a new PMwCAS target to this descriptor.
   *
   * @tparam T a class of a target
   * @param addr a target memory address
   * @param old_val an expected value of a target field
   * @param new_val an inserting value into a target field
   * @param fence a flag for controling std::memory_order.
   * @retval true if target registration succeeds
   * @retval false if this descriptor is already full
   */
  template <class T>
  constexpr void
  AddPMwCASTarget(  //
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
   * @retval true if a PMwCAS operation succeeds
   * @retval false if a PMwCAS operation fails
   */
  auto
  PMwCAS()  //
      -> bool
  {
    const PMwCASField desc_addr{this, kDescriptorFlag};
    constexpr size_t kStatusSize = 1;

    // initialize and persist PMwCAS status
    status_ = DescStatus::kUndecided;
    pmem_persist(this, sizeof(PMwCASDescriptor));
    auto succeeded = true;

    // serialize PMwCAS operations by embedding a descriptor
    size_t embedded_count = 0;

    for (size_t i = 0; i < target_count_; ++i, ++embedded_count) {
      if (!targets_[i].EmbedDescriptor(desc_addr)) {
        // if a target field has been already updated, PMwCAS fails
        succeeded = false;
        break;
      }
    }

    if (succeeded) {
      for (size_t i = 0; i < target_count_; ++i) {
        targets_[i].Flush();
      }
      status_ = DescStatus::kSucceeded;
      pmem_flush(&status_, kStatusSize);
      pmem_drain();
    }

    // complete PMwCAS
    if (succeeded) {
      for (size_t i = 0; i < embedded_count; ++i) {
        targets_[i].RedoPMwCAS();
      }
    } else {
      for (size_t i = 0; i < embedded_count; ++i) {
        targets_[i].UndoPMwCAS();
      }
    }

    target_count_ = 0;
    status_ = DescStatus::kFinished;
    pmem_persist(&target_count_, sizeof(size_t) + kStatusSize);

    return succeeded;
  }

  void
  CompletePMwCAS()
  {
    const PMwCASField desc_addr{this, kDescriptorFlag};
    constexpr size_t kStatusSize = 1;

    // roll forward or roll back PMwCAS
    // when the status is Finished, do nothing
    if (status_ != DescStatus::kFinished) {
      const auto succeeded = (status_ == DescStatus::kSucceeded);
      for (size_t i = 0; i < target_count_; ++i) {
        targets_[i].Recover(succeeded, desc_addr);
      }
    }

    // change status and persist descriptor
    status_ = DescStatus::kFinished;
    pmem_flush(&status_, kStatusSize);
    pmem_drain();
  }

 private:
  /*####################################################################################
   * Internal constants
   *##################################################################################*/

  /// the maximum number of retries for preventing busy loops.
  static constexpr size_t kRetryNum = 10UL;

  /// a sleep time for preventing busy loops.
  static constexpr auto kShortSleep = std::chrono::microseconds{10};

  /// flag for descriptor.
  static constexpr auto kDescriptorFlag = true;

  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// Target entries of PMwCAS
  PMwCASTarget targets_[kPMwCASCapacity];

  /// The number of registered PMwCAS targets
  size_t target_count_{0};

  /// PMwCAS descriptor status
  DescStatus status_{DescStatus::kFinished};
};

}  // namespace dbgroup::atomic::pmwcas

#endif  // PMWCAS_PMWCAS_DESCRIPTOR_HPP
