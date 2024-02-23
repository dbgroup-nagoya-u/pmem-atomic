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

#ifndef PMWCAS_COMPONENT_PMWCAS_WORD_HPP
#define PMWCAS_COMPONENT_PMWCAS_WORD_HPP

// C++ standard libraries
#include <atomic>
#include <cstring>

// local sources
#include "common.hpp"

namespace dbgroup::atomic::pmwcas::component
{
/**
 * @brief A class that represents a PMwCAS target field.
 *
 */
class PMwCASWord
{
 public:
  /*####################################################################################
   * Public constructors and assignment operators
   *##################################################################################*/

  /**
   * @brief Construct an empty field for PMwCAS.
   *
   */
  constexpr PMwCASWord() = default;

  /**
   * @brief Construct a PMwCAS field with given data.
   *
   * @tparam T A target class to be embedded.
   * @param target_data Target data to be embedded.
   * @param is_pmwcas_descriptor A flag to indicate this field contains a descriptor.
   */
  template <class T>
  constexpr explicit PMwCASWord(  //
      T target_data,
      bool is_pmwcas_descriptor = false)
  {
    // static check to validate PMwCAS targets
    static_assert(sizeof(T) == kWordSize);  // NOLINT
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_copy_constructible_v<T>);
    static_assert(std::is_move_constructible_v<T>);
    static_assert(std::is_copy_assignable_v<T>);
    static_assert(std::is_move_assignable_v<T>);
    static_assert(CanPMwCAS<T>());

    memcpy(&word_, &target_data, kWordSize);
    word_ |= static_cast<uint64_t>(is_pmwcas_descriptor) << kPMwCASFlagPos;
  }

  constexpr PMwCASWord(const PMwCASWord &) = default;
  constexpr PMwCASWord(PMwCASWord &&) = default;

  constexpr auto operator=(const PMwCASWord &obj) -> PMwCASWord & = default;
  constexpr auto operator=(PMwCASWord &&) -> PMwCASWord & = default;

  /*####################################################################################
   * Public destructor
   *##################################################################################*/

  /**
   * @brief Destroy the PMwCASWord object.
   *
   */
  ~PMwCASWord() = default;

  /*####################################################################################
   * Public operators
   *##################################################################################*/

  auto
  operator==(const PMwCASWord &obj) const  //
      -> bool
  {
    return word_ == obj.word_;
  }

  auto
  operator!=(const PMwCASWord &obj) const  //
      -> bool
  {
    return word_ != obj.word_;
  }

  /*####################################################################################
   * Public getters/setters
   *##################################################################################*/

  /**
   * @retval true if this field contains an intermediate state.
   * @retval false otherwise.
   */
  [[nodiscard]] constexpr auto
  IsIntermediate() const  //
      -> bool
  {
    if constexpr (kUseDirtyFlag) {
      return word_ & (kPMwCASFlag | kDirtyFlag);
    } else {
      return word_ & kPMwCASFlag;
    }
  }

  /**
   * @retval true if this field may not be persisted.
   * @retval false otherwise.
   */
  [[nodiscard]] constexpr auto
  IsNotPersisted() const  //
      -> bool
  {
    if constexpr (kUseDirtyFlag) {
      return word_ & kDirtyFlag;
    } else {
      return false;
    }
  }

  /**
   * @tparam T An expected class of data.
   * @return Data stored in this field.
   */
  template <class T>
  [[nodiscard]] constexpr auto
  GetTargetData() const  //
      -> T
  {
    if constexpr (std::is_same_v<T, uint64_t>) {
      return word_;
    } else if constexpr (std::is_pointer_v<T>) {
      return reinterpret_cast<T>(word_);
    } else {
      T data;
      memcpy(static_cast<void *>(&data), this, kWordSize);
      return data;
    }
  }

  /**
   * @brief Set a dirty flag.
   *
   */
  void
  SetDirtyFlag()
  {
    word_ |= kDirtyFlag;
  }

  /**
   * @brief Clear a dirty flag.
   *
   */
  void
  ClearDirtyFlag()
  {
    word_ &= ~kDirtyFlag;
  }

 private:
  /*####################################################################################
   * Internal constants
   *##################################################################################*/

  /// @brief The position of a PMwCAS descriptor flag.
  static constexpr size_t kPMwCASFlagPos = 63;

  /// @brief The position of a dirty flag.
  static constexpr size_t kDirtyFlagPos = 62;

  /// @brief A PMwCAS descriptor flag.
  static constexpr uint64_t kPMwCASFlag = 1UL << kPMwCASFlagPos;

  /// @brief A dirty flag.
  static constexpr uint64_t kDirtyFlag = 1UL << kDirtyFlagPos;

  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// @brief An actual target data.
  uint64_t word_{};
};

// CAS target words must be one word
static_assert(sizeof(PMwCASWord) == kWordSize);

}  // namespace dbgroup::atomic::pmwcas::component

#endif  // PMWCAS_COMPONENT_PMWCAS_WORD_HPP
