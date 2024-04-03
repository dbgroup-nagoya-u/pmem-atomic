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
#include "pmem/atomic/pmwcas_descriptor.hpp"

// C++ standard libraries
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

// external system libraries
#include <libpmem.h>
#include <libpmemobj.h>

// external libraries
#include "gtest/gtest.h"

// library headers
#include "pmem/atomic/atomic.hpp"

// local sources
#include "common.hpp"

namespace dbgroup::pmem::atomic::test
{
// prepare a temporary directory
auto *const env = testing::AddGlobalTestEnvironment(new TmpDirManager);

class PMwCASDescriptorFixture : public ::testing::Test
{
 protected:
  /*############################################################################
   * Type aliases
   *##########################################################################*/

  using Target = uint64_t;

  /*############################################################################
   * Constants
   *##########################################################################*/

  static constexpr char kPoolName[] = "pmem_atomic_pmwcas_descriptor_test";
  static constexpr char kLayout[] = "pmem_atomic_pmwcas_descriptor_test";
  static constexpr size_t kTargetFieldNum = kPMwCASCapacity * kTestThreadNum;

  /*############################################################################
   * Setup/Teardown
   *##########################################################################*/

  void
  SetUp() override
  {
    constexpr size_t kArraySize = kWordSize * kTargetFieldNum;
    constexpr size_t kDescPoolSize = kTestThreadNum * sizeof(PMwCASDescriptor);
    constexpr size_t kPoolSize = PMEMOBJ_MIN_POOL + kArraySize + kDescPoolSize;

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
    auto &&root = pmemobj_root(pop_, kArraySize);
    target_fields_ = reinterpret_cast<Target *>(pmemobj_direct(root));
    for (size_t i = 0; i < kTargetFieldNum; ++i) {
      target_fields_[i] = 0UL;
    }
    pmem_persist(target_fields_, kArraySize);
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
  VerifyPMwCAS(  //
      const size_t thread_num)
  {
    // run a function over multi-threads
    std::vector<std::thread> threads{};
    std::mt19937_64 rand_engine(kRandomSeed);
    for (size_t i = 0; i < thread_num; ++i) {
      threads.emplace_back(&PMwCASDescriptorFixture::PMwCASRandomly, this, rand_engine());
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
    size_t sum = 0;
    for (size_t i = 0; i < kTargetFieldNum; ++i) {
      sum += target_fields_[i];
    }
    EXPECT_EQ(kExecNum * thread_num * kPMwCASCapacity, sum);
  }

 private:
  /*############################################################################
   * Internal utility functions
   *##########################################################################*/

  void
  PMwCASRandomly(  //
      const size_t rand_seed)
  {
    // prepare target indices
    std::mt19937_64 rand_engine{rand_seed};
    std::vector<std::vector<size_t>> operations;
    operations.reserve(kExecNum);
    for (size_t i = 0; i < kExecNum; ++i) {
      std::vector<size_t> targets;
      targets.reserve(kPMwCASCapacity);
      while (targets.size() < kPMwCASCapacity) {
        size_t idx = id_dist_(rand_engine);
        const auto iter = std::find(targets.begin(), targets.end(), idx);
        if (iter == targets.end()) {
          targets.emplace_back(idx);
        }
      }
      std::sort(targets.begin(), targets.end());
      operations.emplace_back(std::move(targets));
    }

    {  // wait for a main thread to release a lock
      std::unique_lock lock{mtx_};
      ++ready_num_;
      cond_.wait(lock, [this] { return test_ready_; });
    }

    // prepare descriptor
    PMEMoid oid{OID_NULL};
    if (pmemobj_zalloc(pop_, &oid, sizeof(PMwCASDescriptor), 0) != 0) {
      throw std::runtime_error{pmemobj_errormsg()};
    }
    auto *desc = reinterpret_cast<PMwCASDescriptor *>(pmemobj_direct(oid));
    desc->Initialize();

    // run PMwCAS
    for (auto &&targets : operations) {
      while (true) {
        for (auto &&idx : targets) {
          auto *addr = &(target_fields_[idx]);
          const auto cur_val = PLoad(addr);
          desc->Add(addr, cur_val, cur_val + 1);
        }
        if (desc->PMwCAS()) break;
      }
    }
    pmemobj_free(&oid);
  }

  /*############################################################################
   * Internal member variables
   *##########################################################################*/

  PMEMobjpool *pop_{nullptr};

  Target *target_fields_{nullptr};

  std::uniform_int_distribution<size_t> id_dist_{0, kPMwCASCapacity - 1};

  std::atomic_size_t ready_num_{0};

  std::mutex mtx_{};

  std::condition_variable cond_{};

  bool test_ready_{false};
};

/*##############################################################################
 * Unit test definitions
 *############################################################################*/

TEST_F(PMwCASDescriptorFixture, PMwCASWithSingleThreadCorrectlyIncrementTargets)
{  //
  VerifyPMwCAS(1);
}

TEST_F(PMwCASDescriptorFixture, PMwCASWithMultiThreadsCorrectlyIncrementTargets)
{
  VerifyPMwCAS(kTestThreadNum);
}

}  // namespace dbgroup::pmem::atomic::test
