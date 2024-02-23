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
 * @brief Read a value from a given memory address.
 *
 * @tparam T An expected class of a target field.
 * @param addr A target memory address to read.
 * @param fence A flag for controling std::memory_order.
 * @return A read value.
 * @note If a memory address is included in PMwCAS target fields, it must be read
 * using this function.
 */
template <class T>
inline auto
Read(  //
    const void *addr,
    const std::memory_order fence = std::memory_order_seq_cst)  //
    -> T
{
  using PMwCASWord = component::PMwCASWord;

  const auto *target_addr = static_cast<const std::atomic<PMwCASWord> *>(addr);
  PMwCASWord target_word{};
  while (true) {
    for (size_t i = 1; true; ++i) {
      target_word = target_addr->load(fence);
      if (!target_word.IsIntermediate()) return target_word.GetTargetData<T>();
      if (i > kRetryNum) break;
      PMWCAS_SPINLOCK_HINT
    }

    // wait to prevent busy loop
    std::this_thread::sleep_for(kShortSleep);
  }
}

namespace component
{
/**
 * @brief A class to manage a PMwCAS (multi-words compare-and-swap) operation.
 *
 */
class alignas(kPMEMLineSize) PMwCASDescriptor
{
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
   * @return The number of targets added to PMwCAS.
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
   * @brief Add a new PMwCAS target to this descriptor.
   *
   * @tparam T A class of a target.
   * @param addr A target memory address.
   * @param old_val An expected value of a target field.
   * @param new_val An inserting value into a target field.
   * @param fence A flag for controling std::memory_order.
   */
  template <class T>
  constexpr void
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
  auto
  PMwCAS()  //
      -> bool
  {
    const size_t desc_size = kHeaderSize + kWordSize + sizeof(PMwCASTarget) * target_count_;

    // initialize and persist PMwCAS status
    status_ = DescStatus::kFailed;
    pmem_persist(this, desc_size);

    // serialize PMwCAS operations by embedding a descriptor
    size_t embedded_count = 0;
    for (size_t i = 0; i < target_count_; ++i, ++embedded_count) {
      if (!targets_[i].EmbedDescriptor(desc_addr_)) break;
    }

    // complete PMwCAS
    if (embedded_count < target_count_) {
      // MwCAS failed, so revert changes
      for (size_t i = 0; i < embedded_count; ++i) {
        targets_[i].Undo();
      }
      pmem_drain();

      // reset the descriptor
      status_ = DescStatus::kCompleted;
      target_count_ = 0;
      return false;
    }

    // MwCAS succeeded, so persist the embedded descriptors for fault-tolerance
    for (size_t i = 0; i < target_count_; ++i) {
      targets_[i].Flush();
    }
    status_ = DescStatus::kSucceeded;
    pmem_flush(this, kHeaderSize);
    pmem_drain();

    // update the target address with the desired values
    for (size_t i = 0; i < target_count_; ++i) {
      targets_[i].Redo();
    }
    pmem_drain();

    // reset the descriptor
    status_ = DescStatus::kCompleted;
    target_count_ = 0;
    return true;
  }

  /**
   * @brief Initialize this descriptor and perform recovery if needed.
   *
   */
  void
  Initialize()
  {
    desc_addr_ = PMwCASWord{pmemobj_oid(this).off, kIsDescriptor};

    // roll forward or roll back PMwCAS if needed
    if (status_ != DescStatus::kCompleted) {
      const auto succeeded = (status_ == DescStatus::kSucceeded);
      for (size_t i = 0; i < target_count_; ++i) {
        targets_[i].Recover(succeeded, desc_addr_);
      }
    }

    // change status and persist a descriptor
    status_ = DescStatus::kCompleted;
    target_count_ = 0;
    pmem_flush(this, kHeaderSize + kWordSize);
  }

 private:
  /*####################################################################################
   * Internal constants
   *##################################################################################*/

  /// @brief The size of PMwCAS progress states in bytes.
  static constexpr size_t kHeaderSize = 2 * kWordSize;

  /// @brief A flag to indicate the PMwCAS descriptor.
  static constexpr bool kIsDescriptor = true;

  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// @brief The current state of a PMwCAS operation.
  DescStatus status_{DescStatus::kCompleted};

  /// @brief The number of targets added to PMwCAS.
  size_t target_count_{0};

  /// @brief The address of this descriptor for identification.
  PMwCASWord desc_addr_{0UL, kIsDescriptor};

  /// @brief Target instances of PMwCAS.
  PMwCASTarget targets_[kPMwCASCapacity];
};

}  // namespace component
}  // namespace dbgroup::atomic::pmwcas

#endif  // PMWCAS_PMWCAS_DESCRIPTOR_HPP
