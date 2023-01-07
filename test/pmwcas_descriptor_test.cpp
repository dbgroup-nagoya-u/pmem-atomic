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

#include "pmwcas/component/pmwcas_descriptor.hpp"

#include <future>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <utility>
#include <vector>

#include "common.hpp"
#include "gtest/gtest.h"

namespace dbgroup::atomic::pmwcas::test
{
class PMwCASDescriptorFixture : public ::testing::Test
{
 protected:
  /*####################################################################################
   * Setup/Teardown
   *##################################################################################*/

  void
  SetUp() override
  {
    for (size_t i = 0; i < kTargetFieldNum; ++i) {
      target_fields_[i] = 0UL;
    }
  }

  void
  TearDown() override
  {
  }

  /*####################################################################################
   * Functions for verification
   *##################################################################################*/

  void
  VerifyPMwCAS(const size_t thread_num)
  {
    RunPMwCAS(thread_num);

    // check the target fields are correctly incremented
    size_t sum = 0;
    for (auto &&target : target_fields_) {
      sum += target;
    }

    EXPECT_EQ(kExecNum * thread_num * kPMwCASCapacity, sum);
  }

 private:
  /*####################################################################################
   * Internal constants
   *##################################################################################*/

  static constexpr size_t kExecNum = 1e6;
  static constexpr size_t kTargetFieldNum = kPMwCASCapacity * kThreadNum;
  static constexpr size_t kRandomSeed = 20;

  /*####################################################################################
   * Internal type aliases
   *##################################################################################*/

  using Target = uint64_t;
  using PMwCASTargets = std::vector<size_t>;

  /*####################################################################################
   * Internal utility functions
   *##################################################################################*/

  void
  RunPMwCAS(const size_t thread_num)
  {
    constexpr std::chrono::milliseconds kSleepMS{100};
    std::vector<std::thread> threads;

    {  // create a lock to prevent workers from executing
      const std::unique_lock<std::shared_mutex> guard{worker_lock_};

      // run a function over multi-threads
      std::mt19937_64 rand_engine(kRandomSeed);
      for (size_t i = 0; i < thread_num; ++i) {
        const auto rand_seed = rand_engine();
        threads.emplace_back(&PMwCASDescriptorFixture::PMwCASRandomly, this, rand_seed);
      }

      // wait for all workers to finish initialization
      std::this_thread::sleep_for(kSleepMS);
      const std::unique_lock<std::shared_mutex> lock{main_lock_};
    }

    // wait for all workers to finish
    for (auto &&t : threads) t.join();
  }

  void
  PMwCASRandomly(const size_t rand_seed)
  {
    std::vector<PMwCASTargets> operations;
    operations.reserve(kExecNum);

    {  // create a lock to prevent a main thread
      const std::shared_lock<std::shared_mutex> guard{main_lock_};

      // prepare operations to be executed
      std::mt19937_64 rand_engine{rand_seed};
      for (size_t i = 0; i < kExecNum; ++i) {
        // select PMwCAS target fields randomly
        PMwCASTargets targets;
        targets.reserve(kPMwCASCapacity);
        while (targets.size() < kPMwCASCapacity) {
          size_t idx = id_dist_(rand_engine);
          const auto iter = std::find(targets.begin(), targets.end(), idx);
          if (iter == targets.end()) {
            targets.emplace_back(idx);
          }
        }
        std::sort(targets.begin(), targets.end());

        // add a new targets
        operations.emplace_back(std::move(targets));
      }
    }

    {  // wait for a main thread to release a lock
      const std::shared_lock<std::shared_mutex> lock{worker_lock_};

      for (auto &&targets : operations) {
        // retry until PMwCAS succeeds
        while (true) {
          // register PMwCAS targets
          PMwCASDescriptor desc{};
          for (auto &&idx : targets) {
            auto *addr = &(target_fields_[idx]);
            const auto cur_val = PMwCASDescriptor::Read<Target>(addr);
            const auto new_val = cur_val + 1;
            desc.AddPMwCASTarget(addr, cur_val, new_val);
          }

          // perform PMwCAS
          if (desc.PMwCAS()) break;
        }
      }
    }
  }

  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  Target target_fields_[kTargetFieldNum]{};

  std::uniform_int_distribution<size_t> id_dist_{0, kPMwCASCapacity - 1};

  std::shared_mutex main_lock_;
  std::shared_mutex worker_lock_;
};

/*######################################################################################
 * Unit test definitions
 *####################################################################################*/

TEST_F(PMwCASDescriptorFixture, PMwCASWithSingleThreadCorrectlyIncrementTargets)
{  //
  VerifyPMwCAS(1);
}

TEST_F(PMwCASDescriptorFixture, PMwCASWithMultiThreadsCorrectlyIncrementTargets)
{
  VerifyPMwCAS(kThreadNum);
}

}  // namespace dbgroup::atomic::pmwcas::test
