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
#include <cstdint>
#include <memory>

// external system libraries
#include <libpmemobj.h>

// local sources
#include "pmwcas/component/common.hpp"

namespace dbgroup::pmem::atomic::component
{
/**
 * @brief A class to represent a PMwCAS target.
 *
 */
class PMwCASTarget
{
 public:
  /*############################################################################
   * Public constructors and assignment operators
   *##########################################################################*/

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
  PMwCASTarget(  //
      void *addr,
      const T old_val,
      const T new_val,
      const std::memory_order fence)
      : oid_{pmemobj_oid(addr)},
        old_val_{ToUInt64(old_val)},
        new_val_{ToUInt64(new_val)},
        fence_{fence}
  {
    static_assert(IsAtomic<T>());
  }

  constexpr PMwCASTarget(const PMwCASTarget &) = default;
  constexpr PMwCASTarget(PMwCASTarget &&) = default;

  constexpr auto operator=(const PMwCASTarget &obj) -> PMwCASTarget & = default;
  constexpr auto operator=(PMwCASTarget &&) -> PMwCASTarget & = default;

  /*############################################################################
   * Public destructor
   *##########################################################################*/

  /**
   * @brief Destroy the PMwCASTarget object.
   *
   */
  ~PMwCASTarget() = default;

  /*############################################################################
   * Public utility functions
   *##########################################################################*/

  /**
   * @brief Embed a descriptor into this target address.
   *
   * @param desc_addr A memory address of a target descriptor.
   * @retval true if the descriptor address is successfully embedded.
   * @retval false otherwise.
   */
  auto EmbedDescriptor(    //
      uint64_t desc_addr)  //
      -> bool;

  /**
   * @brief Flush a value of this target address.
   *
   */
  void Flush();

  /**
   * @brief Update a value of this target address.
   *
   */
  void Redo();

  /**
   * @brief Revert a value of this target address.
   *
   */
  void Undo();

  /**
   * @brief Recover and flush the target.
   *
   */
  void Recover(  //
      bool succeeded,
      uint64_t desc_addr);

 private:
  /*############################################################################
   * Internal member variables
   *##########################################################################*/

  /// @brief A target memory address.
  PMEMoid oid_{};

  /// @brief An expected value of a target field.
  uint64_t old_val_{};

  /// @brief An inserting value into a target field.
  uint64_t new_val_{};

  /// @brief A fence to be inserted when embedding a new value.
  std::memory_order fence_{std::memory_order_seq_cst};
};

}  // namespace dbgroup::pmem::atomic::component

#endif  // PMWCAS_COMPONENT_PMWCAS_TARGET_HPP
