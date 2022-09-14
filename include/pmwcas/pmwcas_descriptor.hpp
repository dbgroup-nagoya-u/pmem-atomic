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

#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <utility>

#include "component/pmwcas_target.hpp"

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
  constexpr auto operator=(const PMwCASDescriptor &obj) -> PMwCASDescriptor & = default;
  constexpr PMwCASDescriptor(PMwCASDescriptor &&) = default;
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
   * @return a read value
   */
  template <class T>
  static auto
  Read(const void *addr)  //
      -> T
  {
    const auto *target_addr = static_cast<const std::atomic<PMwCASField> *>(addr);

    PMwCASField target_word{};
    for (size_t i = 0; i < kRetryNum; ++i) {
      target_word = target_addr->load(std::memory_order_relaxed);
      if (!target_word.IsPMwCASDescriptor()) return target_word.GetTargetData<T>();
      SPINLOCK_HINT
    }

    // wait to prevent busy loop
    std::this_thread::sleep_for(kShortSleep);
  }

  /**
   * @brief Add a new PMwCAS target to this descriptor.
   *
   * @tparam T a class of a target
   * @param addr a target memory address
   * @param old_val an expected value of a target field
   * @param new_val an inserting value into a target field
   * @retval true if target registration succeeds
   * @retval false if this descriptor is already full
   */
  template <class T>
  constexpr auto
  AddPMwCASTarget(  //
      void *addr,
      const T old_val,
      const T new_val)  //
      -> bool
  {
    if (target_count_ == kPMwCASCapacity) {
      return false;
    }
    targets_[target_count_++] = PMwCASTarget{addr, old_val, new_val};
    return true;
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
    const PMwCASField desc_addr{this, true};

    // initialize PMwCAS status
    status_ = component::kUndecided;
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

    status_ = (succeeded) ? component::kSucceeded : component::kFailed;

    // complete PMwCAS
    for (size_t i = 0; i < embedded_count; ++i) {
      targets_[i].CompletePMwCAS(succeeded);
    }

    status_ = component::kFinished;

    return succeeded;
  }

 private:
  /*####################################################################################
   * Internal constants
   *##################################################################################*/

  /// the maximum number of retries for preventing busy loops.
  static constexpr size_t kRetryNum = 10UL;

  /// a sleep time for preventing busy loops.
  static constexpr auto kShortSleep = std::chrono::microseconds{10};

  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// Target entries of PMwCAS
  PMwCASTarget targets_[kPMwCASCapacity];

  /// The number of registered PMwCAS targets
  size_t target_count_{0};

  component::DescStatus status_{component::kFinished};
};

}  // namespace dbgroup::atomic::pmwcas

#endif  // PMWCAS_PMWCAS_DESCRIPTOR_HPP
