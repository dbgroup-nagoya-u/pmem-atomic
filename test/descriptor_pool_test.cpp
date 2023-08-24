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

#include "pmwcas/descriptor_pool.hpp"

#include <chrono>
#include <future>
#include <iterator>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_set>

#include "common.hpp"
#include "gtest/gtest.h"

namespace dbgroup::atomic::pmwcas::test
{
/*######################################################################################
 * Global constants
 *####################################################################################*/

constexpr std::string_view kTmpPMEMPath = DBGROUP_ADD_QUOTES(DBGROUP_TEST_TMP_PMEM_PATH);
constexpr const char *kPoolName = "pmwcas_descriptor_pool_test";

class DescriptorPoolFixture : public ::testing::Test
{
 protected:
  /*####################################################################################
   * Setup/Teardown
   *##################################################################################*/

  void
  SetUp() override
  {
    try {
      const std::string user_name{std::getenv("USER")};
      std::filesystem::path pool_path{kTmpPMEMPath};
      pool_path /= user_name;
      std::filesystem::create_directories(pool_path);
      pool_path /= kPoolName;
      std::filesystem::remove(pool_path);

      pool_ = std::make_unique<DescriptorPool>(pool_path);
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
      std::terminate();
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
  GetAllDescriptor(const size_t pool_size)
  {
    constexpr std::chrono::milliseconds kSleepMS{100};

    is_ready_ = false;
    std::vector<std::thread> threads{};
    std::vector<std::future<PMwCASDescriptor *>> futures{};

    for (size_t i = 0; i < pool_size; ++i) {
      std::promise<PMwCASDescriptor *> p{};
      futures.emplace_back(p.get_future());
      threads.emplace_back(&DescriptorPoolFixture::GetDescriptor, this, std::move(p));
    }

    {
      std::this_thread::sleep_for(kSleepMS);
      [[maybe_unused]] std::lock_guard s_guard{s_mtx_};
    }

    std::promise<PMwCASDescriptor *> extra_p{};
    auto &&extra_f = extra_p.get_future();
    std::thread extra_t(&DescriptorPoolFixture::GetDescriptor, this, std::move(extra_p));
    auto rc = extra_f.wait_for(std::chrono::milliseconds(3));
    EXPECT_EQ(rc, std::future_status::timeout);

    {
      std::lock_guard x_guard{x_mtx_};
      is_ready_ = true;
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
  /*####################################################################################
   * Internal utility functions
   *##################################################################################*/

  void
  GetDescriptor(std::promise<PMwCASDescriptor *> p)
  {
    std::shared_lock guard{s_mtx_};
    auto *desc = pool_->Get();
    p.set_value(desc);
    {
      std::unique_lock lock{x_mtx_};
      guard.unlock();
      cond_.wait(lock, [this] { return is_ready_; });
    }
  }

  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  std::unique_ptr<DescriptorPool> pool_{nullptr};

  std::mutex x_mtx_{};

  std::shared_mutex s_mtx_{};

  std::condition_variable cond_{};

  bool is_ready_{false};
};

/*######################################################################################
 * Unit test definitions
 *####################################################################################*/

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

}  // namespace dbgroup::atomic::pmwcas::test
