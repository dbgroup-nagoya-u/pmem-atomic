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

// the corresponding header
#include "pmwcas/atomic.hpp"

// C++ standard libraries
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

// external system libraries
#include <libpmem.h>
#include <libpmemobj.h>

// external libraries
#include "gtest/gtest.h"

// local sources
#include "common.hpp"

namespace dbgroup::pmem::atomic::test
{
// prepare a temporary directory
auto *const env = testing::AddGlobalTestEnvironment(new TmpDirManager);

class PmemAtomicFixture : public ::testing::Test
{
 protected:
  /*############################################################################
   * Type aliases
   *##########################################################################*/

  using Target = uint64_t;

  /*############################################################################
   * Constants
   *##########################################################################*/

  static constexpr char kPoolName[] = "pmem_atomic_atomic_test";
  static constexpr char kLayout[] = "pmem_atomic_atomic_test";

  /*############################################################################
   * Setup/Teardown
   *##########################################################################*/

  void
  SetUp() override
  {
    constexpr size_t kPoolSize = PMEMOBJ_MIN_POOL;

    test_ready_ = false;
    ready_num_ = 0;

    // create a persistent pool for testing
    auto &&pool_path = GetTmpPoolPath();
    pool_path /= kPoolName;
    if (std::filesystem::exists(pool_path)) {
      pop_ = pmemobj_open(pool_path.c_str(), kLayout);
    } else {
      pop_ = pmemobj_create(pool_path.c_str(), kLayout, kPoolSize, kModeRW);
    }

    // initialize target fields
    auto &&root = pmemobj_root(pop_, kWordSize);
    target_ = reinterpret_cast<Target *>(pmemobj_direct(root));
    *target_ = 0UL;
    pmem_persist(target_, kWordSize);
  }

  void
  TearDown() override
  {
    if (pop_ != nullptr) {
      pmemobj_close(pop_);
    }
  }

  /*############################################################################
   * Functions for verification
   *##########################################################################*/

  void
  VerifyPCAS(  //
      const size_t thread_num)
  {
    // run a function over multi-threads
    std::vector<std::thread> threads{};
    for (size_t i = 0; i < thread_num; ++i) {
      threads.emplace_back(&PmemAtomicFixture::PCASRandomly, this);
    }

    {  // wait for all workers to finish initialization
      while (ready_num_ < thread_num) {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
      }
      std::lock_guard x_guard{mtx_};
      test_ready_ = true;
    }
    cond_.notify_all();
    for (auto &&t : threads) {
      t.join();
    }

    // check the total number of modifications
    size_t sum = *target_;
    EXPECT_EQ(kExecNum * thread_num, sum);
  }

 private:
  /*############################################################################
   * Internal utility functions
   *##########################################################################*/

  void
  PCASRandomly()
  {
    {  // wait for a main thread to release a lock
      std::unique_lock lock{mtx_};
      ++ready_num_;
      cond_.wait(lock, [this] { return test_ready_; });
    }

    for (size_t i = 0; i < kExecNum; ++i) {
      auto cur_val = PLoad(target_);
      while (!PCAS(target_, cur_val, cur_val + 1)) {
        // continue until PCAS succeeds
      }
    }
  }

  /*############################################################################
   * Internal member variables
   *##########################################################################*/

  PMEMobjpool *pop_{nullptr};

  Target *target_{nullptr};

  std::atomic_size_t ready_num_{0};

  std::mutex mtx_{};

  std::condition_variable cond_{};

  bool test_ready_{false};
};

/*##############################################################################
 * Unit test definitions
 *############################################################################*/

TEST_F(PmemAtomicFixture, PCASWithSingleThreadCorrectlyIncrementTargets)
{  //
  VerifyPCAS(1);
}

TEST_F(PmemAtomicFixture, PCASWithMultiThreadsCorrectlyIncrementTargets)
{
  VerifyPCAS(kTestThreadNum);
}

}  // namespace dbgroup::pmem::atomic::test
