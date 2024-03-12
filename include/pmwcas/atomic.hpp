/*
 * Copyright 2024 Database Group, Nagoya University
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

#ifndef PMWCAS_ATOMIC_HPP
#define PMWCAS_ATOMIC_HPP

// C++ standard libraries
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

// external system libraries
#include <libpmem.h>

// local sources
#include "component/common.hpp"

namespace dbgroup::pmem::atomic
{
/**
 * @tparam T A class of target words.
 * @param addr An address of a target word.
 * @param order A memory barrier for this operation.
 * @return The current value of a given address.
 */
template <class T>
inline auto
PLoad(  //
    T *addr,
    const std::memory_order order = std::memory_order_seq_cst)  //
    -> T
{
  auto *word_addr = reinterpret_cast<std::atomic_uint64_t *>(addr);
  auto word = word_addr->load(order);
  component::ResolveIntermediateState(word_addr, word);

  if constexpr (std::is_same_v<T, uint64_t>) {
    return word;
  } else if constexpr (std::is_pointer_v<T>) {
    return reinterpret_cast<T>(word);
  } else {
    T ret;
    std::memcpy(static_cast<void *>(&ret), &word, kWordSize);
    return ret;
  }
}

/**
 * @tparam T A class of target words.
 * @param[in] addr An address of a target word.
 * @param[in,out] expected An expected value.
 * @param[in] desired A desired value.
 * @param[in] success A memory barrier in case CAS succeeds.
 * @param[in] failure A memory barrier in case CAS fails.
 * @retval true if this PCAS operation succeeds.
 * @retval false otherwise.
 */
template <class T>
inline auto
PCAS(  //
    T *addr,
    T &expected,
    const T desired,
    const std::memory_order success,
    const std::memory_order failure)  //
    -> bool
{
  constexpr auto kMORelax = std::memory_order_relaxed;

  uint64_t uint_exp;
  uint64_t uint_des;
  if constexpr (std::is_pointer_v<T>) {
    uint_exp = reinterpret_cast<uint64_t>(expected);
    uint_des = reinterpret_cast<uint64_t>(desired);
  } else {
    std::memcpy(static_cast<void *>(&uint_exp), &expected, kWordSize);
    std::memcpy(static_cast<void *>(&uint_des), &desired, kWordSize);
  }

  const auto orig_expected = uint_exp;
  auto *word_addr = reinterpret_cast<std::atomic_uint64_t *>(addr);
  auto dirty_v = uint_des | kDirtyFlag;
  while (!word_addr->compare_exchange_weak(uint_exp, dirty_v, kMORelax, failure)) {
    if (uint_exp & kDirtyFlag) {
      component::ResolveIntermediateState(word_addr, uint_exp);
    }
    if (uint_exp != orig_expected) {
      if constexpr (std::is_pointer_v<T>) {
        expected = reinterpret_cast<T>(uint_exp);
      } else {
        std::memcpy(static_cast<void *>(&expected), &uint_exp, kWordSize);
      }
      return false;
    }
  }

  pmem_persist(addr, kWordSize);
  word_addr->compare_exchange_strong(dirty_v, uint_des, success, kMORelax);
  return true;
}

/**
 * @tparam T A class of target words.
 * @param[in] addr An address of a target word.
 * @param[in,out] expected An expected value.
 * @param[in] desired A desired value.
 * @param[in] success A memory barrier for CAS operations.
 * @retval true if this PCAS operation succeeds.
 * @retval false otherwise.
 */
template <class T>
inline auto
PCAS(  //
    T *addr,
    T &expected,
    const T desired,
    const std::memory_order order = std::memory_order_seq_cst)  //
    -> bool
{
  return PCAS(addr, expected, desired, order,
              order == std::memory_order_acq_rel   ? std::memory_order_acquire
              : order == std::memory_order_release ? std::memory_order_relaxed
                                                   : order);
}

}  // namespace dbgroup::pmem::atomic

#endif  // PMWCAS_ATOMIC_HPP
