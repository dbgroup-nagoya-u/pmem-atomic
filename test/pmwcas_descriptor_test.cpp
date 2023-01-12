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
#include "pmwcas/component/pmwcas_descriptor.hpp"

// C++ standard libraries
#include <filesystem>
#include <future>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <utility>
#include <vector>

// external system libraries
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

// external libraries
#include "gtest/gtest.h"

// local sources
#include "common.hpp"

namespace dbgroup::atomic::pmwcas::test
{
/*######################################################################################
 * Global constants
 *####################################################################################*/

constexpr std::string_view kTmpPMEMPath = DBGROUP_ADD_QUOTES(DBGROUP_TEST_TMP_PMEM_PATH);
constexpr const char *kPoolName = "pmwcas_pmwcas_descriptor_test";
constexpr const char *kLayout = "target";
constexpr size_t kExecNum = 1e6;
constexpr size_t kTargetFieldNum = kPMwCASCapacity * kThreadNum;
constexpr size_t kRandomSeed = 20;
constexpr auto kModeRW = S_IRUSR | S_IWUSR;  // NOLINT

class PMwCASDescriptorFixture : public ::testing::Test
{
 protected:
  /*####################################################################################
   * Internal classes
   *##################################################################################*/

  using Target = uint64_t;

  struct PMEMRoot {
    ::pmem::obj::p<Target> target_fields[kTargetFieldNum]{};
  };

  /*####################################################################################
   * Setup/Teardown
   *##################################################################################*/

  void
  SetUp() override
  {
    try {
      // create a user directory for testing
      const std::string user_name{std::getenv("USER")};
      std::filesystem::path pool_path{kTmpPMEMPath};
      pool_path /= user_name;
      std::filesystem::create_directories(pool_path);
      pool_path /= kPoolName;
      std::filesystem::remove(pool_path);

      // create a persistent pool for testing
      constexpr size_t kSize = PMEMOBJ_MIN_POOL * 64;
      pool_ = ::pmem::obj::pool<PMEMRoot>::create(pool_path, kLayout, kSize, kModeRW);

      // allocate regions on persistent memory
      ::pmem::obj::transaction::run(pool_, [&] {
        auto &&root = pool_.root();
        for (size_t i = 0; i < kTargetFieldNum; ++i) {
          root->target_fields[i] = 0UL;
        }
      });
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
      std::terminate();
    }
  }

  void
  TearDown() override
  {
    pool_.close();
  }

  /*####################################################################################
   * Functions for verification
   *##################################################################################*/

  void
  VerifyPMwCAS(const size_t thread_num)
  {
    RunPMwCAS(thread_num);

    // check the target fields are correctly incremented
    auto &&root = pool_.root();
    size_t sum = 0;
    for (size_t i = 0; i < kTargetFieldNum; ++i) {
      sum += root->target_fields[i].get_ro();
    }

    EXPECT_EQ(kExecNum * thread_num * kPMwCASCapacity, sum);
  }

 private:
  /*####################################################################################
   * Internal type aliases
   *##################################################################################*/

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
      auto &&root = pool_.root();

      for (auto &&targets : operations) {
        // retry until PMwCAS succeeds
        while (true) {
          // register PMwCAS targets
          PMwCASDescriptor desc{};
          for (auto &&idx : targets) {
            auto *addr = &(root->target_fields[idx]);
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

  ::pmem::obj::pool<PMEMRoot> pool_{};

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
