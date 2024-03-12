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

// the corresponding header
#include "pmem/atomic/descriptor_pool.hpp"

// C++ standard libraries
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <future>
#include <iterator>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

// external libraries
#include "gtest/gtest.h"
#include "thread/common.hpp"

// local sources
#include "common.hpp"

namespace dbgroup::pmem::atomic::test
{
// prepare a temporary directory
auto *const env = testing::AddGlobalTestEnvironment(new TmpDirManager);

class DescriptorPoolFixture : public ::testing::Test
{
 protected:
  /*############################################################################
   * Constants
   *##########################################################################*/

  static constexpr char kPoolName[] = "pmem_atomic_descriptor_pool_test";
  static constexpr auto kMaxThreadNum = ::dbgroup::thread::kMaxThreadNum;

  /*############################################################################
   * Setup/Teardown
   *##########################################################################*/

  void
  SetUp() override
  {
    auto &&pool_path = GetTmpPoolPath();
    pool_path /= kPoolName;
    pool_ = std::make_unique<DescriptorPool>(pool_path);
  }

  void
  TearDown() override
  {
  }

  /*############################################################################
   * Functions for verification
   *##########################################################################*/

  void
  GetOneDescriptor()
  {
    auto f = [&]() {
      auto *desc_1 = pool_->Get();
      auto *desc_2 = pool_->Get();

      // check if they are the same descriptor
      EXPECT_EQ(desc_1, desc_2);
    };

    std::thread t{f};
    t.join();
  }

  void
  GetAllDescriptor(  //
      const size_t pool_size)
  {
    test_ready_ = false;
    ready_num_ = 0;

    std::vector<std::thread> threads{};
    std::vector<std::future<PMwCASDescriptor *>> futures{};
    for (size_t i = 0; i < pool_size; ++i) {
      std::promise<PMwCASDescriptor *> p{};
      futures.emplace_back(p.get_future());
      threads.emplace_back(&DescriptorPoolFixture::GetDescriptor, this, std::move(p));
    }

    // wait for all workers to finish initialization
    while (ready_num_ < pool_size) {
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    std::promise<PMwCASDescriptor *> extra_p{};
    auto &&extra_f = extra_p.get_future();
    std::thread extra_t(&DescriptorPoolFixture::GetDescriptor, this, std::move(extra_p));
    auto rc = extra_f.wait_for(std::chrono::milliseconds(1));
    EXPECT_EQ(rc, std::future_status::timeout);

    {
      std::lock_guard x_guard{mtx_};
      test_ready_ = true;
    }
    cond_.notify_all();

    std::vector<PMwCASDescriptor *> results{};
    results.reserve(pool_size);
    for (auto &&future : futures) {
      results.emplace_back(future.get());
    }
    for (auto &t : threads) {
      t.join();
    }

    std::unordered_set<PMwCASDescriptor *> distinct(std::begin(results), std::end(results));
    EXPECT_EQ(distinct.size(), pool_size);

    extra_t.join();
  }

 private:
  /*############################################################################
   * Internal utility functions
   *##########################################################################*/

  void
  GetDescriptor(  //
      std::promise<PMwCASDescriptor *> p)
  {
    auto *desc = pool_->Get();
    p.set_value(desc);
    {
      std::unique_lock lock{mtx_};
      ++ready_num_;
      cond_.wait(lock, [this] { return test_ready_; });
    }
  }

  /*############################################################################
   * Internal member variables
   *##########################################################################*/

  std::unique_ptr<DescriptorPool> pool_{nullptr};

  std::atomic_size_t ready_num_{0};

  std::mutex mtx_{};

  std::condition_variable cond_{};

  bool test_ready_{false};
};

/*##############################################################################
 * Unit test definitions
 *############################################################################*/

TEST_F(DescriptorPoolFixture, GetTwoSameDescriptorInOneThread)
{  //
  GetOneDescriptor();
}

TEST_F(DescriptorPoolFixture, GetDifferentDescriptorsInAllThread)
{  //
  GetAllDescriptor(kMaxThreadNum);
}

TEST_F(DescriptorPoolFixture, GetDifferentDescriptorsInAllThreadTwice)
{
  GetAllDescriptor(kMaxThreadNum);
  GetAllDescriptor(kMaxThreadNum);
}

}  // namespace dbgroup::pmem::atomic::test
