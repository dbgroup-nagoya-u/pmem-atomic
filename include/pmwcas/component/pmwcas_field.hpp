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

#include <atomic>

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
  constexpr PMwCASField() : target_bit_arr_{}, pmwcas_flag_{0} {}

  /**
   * @brief Construct a PMwCAS field with given data.
   *
   * @tparam T a target class to be embedded.
   * @param target_data target data to be embedded.
   * @param is_pmwcas_descriptor a flag to indicate this field contains a descriptor.
   */
  template <class T>
  explicit constexpr PMwCASField(  //
      T target_data,
      bool is_pmwcas_descriptor = false)
      : target_bit_arr_{ConvertToUint64(target_data)}, pmwcas_flag_{is_pmwcas_descriptor}
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
  constexpr auto operator=(const PMwCASField &obj) -> PMwCASField & = default;
  constexpr PMwCASField(PMwCASField &&) = default;
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

  constexpr auto
  operator==(const PMwCASField &obj) const  //
      -> bool
  {
    return this->pmwcas_flag_ == obj.pmwcas_flag_  //
           && this->target_bit_arr_ == obj.target_bit_arr_;
  }

  constexpr auto
  operator!=(const PMwCASField &obj) const  //
      -> bool
  {
    return this->pmwcas_flag_ != obj.pmwcas_flag_  //
           || this->target_bit_arr_ != obj.target_bit_arr_;
  }

  /*####################################################################################
   * Public getters/setters
   *##################################################################################*/

  /**
   * @retval true if this field contains a descriptor.
   * @retval false otherwise.
   */
  [[nodiscard]] constexpr auto
  IsPMwCASDescriptor() const  //
      -> bool
  {
    return pmwcas_flag_;
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

  /// An actual target data
  uint64_t target_bit_arr_ : 63;

  /// Representing whether this field contains a PMwCAS descriptor
  uint64_t pmwcas_flag_ : 1;
};

// CAS target words must be one word
static_assert(sizeof(PMwCASField) == kWordSize);

}  // namespace dbgroup::atomic::pmwcas::component

#endif  // PMWCAS_COMPONENT_PMWCAS_FIELD_HPP
