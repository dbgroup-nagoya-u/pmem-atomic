/*
 * Copyright 2022 Database Group, Nagoya University
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

#ifndef PMEM_ATOMIC_DESCRIPTOR_POOL_HPP
#define PMEM_ATOMIC_DESCRIPTOR_POOL_HPP

// C++ standard libraries
#include <string>

// external system libraries
#include <libpmemobj.h>

// local sources
#include "pmem/atomic/atomic.hpp"
#include "pmem/atomic/pmwcas_descriptor.hpp"

namespace dbgroup::pmem::atomic
{
/**
 * @brief A class representing the pool of descriptors.
 *
 */
class DescriptorPool
{
 public:
  /*############################################################################
   * Public constructors and assignment operators
   *##########################################################################*/

  /**
   * @brief Construct a new DescriptorPool object.
   *
   * @param pmem_path The path to a pmemobj pool for PMwCAS.
   * @param layout_name The layout name to distinguish application.
   */
  explicit DescriptorPool(  //
      const std::string &pmem_path,
      const std::string &layout_name = "pmwcas_desc_pool");

  DescriptorPool(const DescriptorPool &) = delete;
  DescriptorPool(DescriptorPool &&) = delete;

  auto operator=(const DescriptorPool &) -> DescriptorPool & = delete;
  auto operator=(DescriptorPool &&) -> DescriptorPool & = delete;

  /*############################################################################
   * Public destructors
   *##########################################################################*/

  /**
   * @brief Destroy the DescriptorPool object.
   *
   */
  ~DescriptorPool();

  /*############################################################################
   * Public utility functions
   *##########################################################################*/

  /**
   * @return The PMwCAS descriptor for the current thread.
   * @note If a thread calls this function multiple times, it will return the
   * same descriptor.
   */
  auto Get()  //
      -> PMwCASDescriptor *;

 private:
  /*############################################################################
   * Internal member variables
   *##########################################################################*/

  /// @brief The pmemobj_pool for holding PMwCAS descriptors.
  PMEMobjpool *pop_{nullptr};

  /// @brief The pool of PMwCAS descriptors.
  PMwCASDescriptor *desc_pool_{nullptr};
};

}  // namespace dbgroup::pmem::atomic

#endif  // PMEM_ATOMIC_DESCRIPTOR_POOL_HPP
