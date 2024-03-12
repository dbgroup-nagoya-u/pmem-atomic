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

#ifndef TEST_COMMON_HPP
#define TEST_COMMON_HPP

// C++ standard libraries
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>

// system libraries
#include <sys/stat.h>
#include <sys/types.h>

// external libraries
#include "gtest/gtest.h"

// local sources
#include "pmwcas/utility.hpp"

/*##############################################################################
 * Global macro
 *############################################################################*/

#define DBGROUP_ADD_QUOTES_INNER(x) #x                     // NOLINT
#define DBGROUP_ADD_QUOTES(x) DBGROUP_ADD_QUOTES_INNER(x)  // NOLINT

/*##############################################################################
 * Global contants
 *############################################################################*/

constexpr size_t kTestThreadNum = DBGROUP_TEST_THREAD_NUM;

constexpr size_t kExecNum = DBGROUP_TEST_EXEC_NUM;

constexpr size_t kRandomSeed = DBGROUP_TEST_RANDOM_SEED;

constexpr std::string_view kTmpPMEMPath = DBGROUP_ADD_QUOTES(DBGROUP_TEST_TMP_PMEM_PATH);

constexpr mode_t kModeRW = S_IRUSR | S_IWUSR;

const std::string_view use_name = std::getenv("USER");

/*##############################################################################
 * Global utilities for persistent memory
 *############################################################################*/

inline auto
GetTmpPoolPath()  //
    -> std::filesystem::path
{
  std::filesystem::path pool_path{kTmpPMEMPath};
  pool_path /= use_name;
  pool_path /= "tmp_test_dir";

  return pool_path;
}

class TmpDirManager : public ::testing::Environment
{
 public:
  // constructor/destructor
  TmpDirManager() = default;
  TmpDirManager(const TmpDirManager &) = default;
  TmpDirManager(TmpDirManager &&) = default;
  TmpDirManager &operator=(const TmpDirManager &) = default;
  TmpDirManager &operator=(TmpDirManager &&) = default;
  ~TmpDirManager() override = default;

  // Override this to define how to set up the environment.
  void
  SetUp() override
  {
    // check the specified path
    if (kTmpPMEMPath.empty() || !std::filesystem::exists(kTmpPMEMPath)) {
      std::cerr << "WARN: The correct path to persistent memory is not set.\n";
      GTEST_SKIP();
    }

    // prepare a temporary directory for testing
    const auto &pool_path = GetTmpPoolPath();
    std::filesystem::remove_all(pool_path);
    std::filesystem::create_directories(pool_path);
  }

  // Override this to define how to tear down the environment.
  void
  TearDown() override
  {
    const auto &pool_path = GetTmpPoolPath();
    std::filesystem::remove_all(pool_path);
  }
};

/*##############################################################################
 * Global utilities for PMwCAS
 *############################################################################*/

/**
 * @brief An example class to represent CAS-updatable data.
 *
 */
class MyClass
{
 public:
  constexpr MyClass() : data_{0}, control_bits_{0} {}

  constexpr MyClass(const MyClass &) = default;
  constexpr MyClass(MyClass &&) = default;

  constexpr auto operator=(const MyClass &) -> MyClass & = default;
  constexpr auto operator=(MyClass &&) -> MyClass & = default;

  ~MyClass() = default;

  constexpr auto
  operator=(const uint64_t value)  //
      -> MyClass &
  {
    data_ = value;
    return *this;
  }

  constexpr auto
  operator==(const MyClass &comp) const  //
      -> bool
  {
    return data_ == comp.data_;
  }

  constexpr auto
  operator!=(const MyClass &comp) const  //
      -> bool
  {
    return data_ != comp.data_;
  }

 private:
  uint64_t data_ : 62;
  uint64_t control_bits_ : 2;
};

namespace dbgroup::pmem::atomic
{
/**
 * @brief Specialization to enable PMwCAS to swap our sample class.
 *
 */
template <>
constexpr auto
CanPCAS<MyClass>()  //
    -> bool
{
  return true;
}

}  // namespace dbgroup::pmem::atomic

#endif  // TEST_COMMON_HPP
