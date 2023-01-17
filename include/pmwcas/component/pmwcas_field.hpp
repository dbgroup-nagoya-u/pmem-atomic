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

#ifndef PMWCAS_COMPONENT_PMWCAS_FIELD_HPP
#define PMWCAS_COMPONENT_PMWCAS_FIELD_HPP

// C++ standard libraries
#include <atomic>
#include <cstring>

// local sources
#include "common.hpp"

namespace dbgroup::atomic::pmwcas::component
{
/**
 * @brief A class to represent a PMwCAS target field.
 *
 */
class PMwCASField
{
 public:
  /*####################################################################################
   * Public constructors and assignment operators
   *##################################################################################*/

  /**
   * @brief Construct an empty field for PMwCAS.
   *
   */
#ifdef PMWCAS_USE_DIRTY_FLAG
  constexpr PMwCASField() : target_bit_arr_{}, pmwcas_flag_{0}, dirty_flag_{0} {}
#else
  constexpr PMwCASField() : target_bit_arr_{}, pmwcas_flag_{0} {}
#endif

  /**
   * @brief Construct a PMwCAS field with given data.
   *
   * @tparam T a target class to be embedded.
   * @param target_data target data to be embedded.
   * @param is_pmwcas_descriptor a flag to indicate this field contains a descriptor.
   * @param is_not_persisted a flag to indicate this field is not persisted.
   */
  template <class T>
  explicit constexpr PMwCASField(  //
      T target_data,
      bool is_pmwcas_descriptor = false)
      : target_bit_arr_{ConvertToUint64(target_data)},
#ifdef PMWCAS_USE_DIRTY_FLAG
        pmwcas_flag_{static_cast<uint64_t>(is_pmwcas_descriptor)},
        dirty_flag_{0}
#else
        pmwcas_flag_{static_cast<uint64_t>(is_pmwcas_descriptor)}
#endif
  {
    // static check to validate PMwCAS targets
    static_assert(sizeof(T) == kWordSize);  // NOLINT
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_copy_constructible_v<T>);
    static_assert(std::is_move_constructible_v<T>);
    static_assert(std::is_copy_assignable_v<T>);
    static_assert(std::is_move_assignable_v<T>);
    static_assert(CanPMwCAS<T>());
  }

  constexpr PMwCASField(const PMwCASField &) = default;
  constexpr PMwCASField(PMwCASField &&) = default;
  constexpr auto operator=(const PMwCASField &obj) -> PMwCASField & = default;
  constexpr auto operator=(PMwCASField &&) -> PMwCASField & = default;

  /*####################################################################################
   * Public destructor
   *##################################################################################*/

  /**
   * @brief Destroy the PMwCASField object.
   *
   */
  ~PMwCASField() = default;

  /*####################################################################################
   * Public operators
   *##################################################################################*/

  auto
  operator==(const PMwCASField &obj) const  //
      -> bool
  {
    return memcmp(this, &obj, sizeof(PMwCASField)) == 0;
  }

  auto
  operator!=(const PMwCASField &obj) const  //
      -> bool
  {
    return memcmp(this, &obj, sizeof(PMwCASField)) != 0;
  }

  /*####################################################################################
   * Public getters/setters
   *##################################################################################*/

  /**
   * @retval true if this field may not be persisted.
   * @retval false otherwise.
   */
  [[nodiscard]] constexpr auto
  IsNotPersisted() const  //
      -> bool
  {
#ifdef PMWCAS_USE_DIRTY_FLAG
    return dirty_flag_;
#else
    return false;
#endif
  }

  /**
   * @retval true if this field contains a descriptor.
   * @retval false otherwise.
   */
  [[nodiscard]] constexpr auto
  IsPMwCASDescriptor() const  //
      -> bool
  {
#ifdef PMWCAS_USE_DIRTY_FLAG
    return pmwcas_flag_ && dirty_flag_;
#else
    return pmwcas_flag_;
#endif
  }

  /**
   * @tparam T an expected class of data.
   * @return data retained in this field.
   */
  template <class T>
  [[nodiscard]] constexpr auto
  GetTargetData() const  //
      -> T
  {
    if constexpr (std::is_same_v<T, uint64_t>) {
      return target_bit_arr_;
    } else if constexpr (std::is_pointer_v<T>) {
      return reinterpret_cast<T>(target_bit_arr_);  // NOLINT
    } else {
      return CASTargetConverter<T>{target_bit_arr_}.target_data;  // NOLINT
    }
  }

  /**
   * @brief Set or remove the dirty flag.
   *
   * @param is_dirty a flag for whether to set the dirty bit.
   */
  void
  SetDirtyFlag(bool is_dirty)
  {
#ifdef PMWCAS_USE_DIRTY_FLAG
    dirty_flag_ = is_dirty;
#endif
  }

 private:
  /*####################################################################################
   * Internal utility functions
   *##################################################################################*/

  /**
   * @brief Conver given data into uint64_t.
   *
   * @tparam T a class of given data.
   * @param data data to be converted.
   * @return data converted to uint64_t.
   */
  template <class T>
  constexpr auto
  ConvertToUint64(const T data)  //
      -> uint64_t
  {
    if constexpr (std::is_same_v<T, uint64_t>) {
      return data;
    } else if constexpr (std::is_pointer_v<T>) {
      return reinterpret_cast<uint64_t>(data);  // NOLINT
    } else {
      return CASTargetConverter<T>{data}.converted_data;  // NOLINT
    }
  }

  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

#ifdef PMWCAS_USE_DIRTY_FLAG
  /// An actual target data
  uint64_t target_bit_arr_ : 62;

  /// Representing whether this field contains a PMwCAS descriptor
  uint64_t pmwcas_flag_ : 1;

  /// A flag for indicating this field may not be persisted.
  uint64_t dirty_flag_ : 1;
#else
  /// An actual target data
  uint64_t target_bit_arr_ : 63;

  /// Representing whether this field contains a PMwCAS descriptor
  uint64_t pmwcas_flag_ : 1;
#endif
};

// CAS target words must be one word
static_assert(sizeof(PMwCASField) == kWordSize);

}  // namespace dbgroup::atomic::pmwcas::component

#endif  // PMWCAS_COMPONENT_PMWCAS_FIELD_HPP
