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

#include <iterator>
#include <shared_mutex>
#include <thread>
#include <unordered_set>

#include "common.hpp"
#include "gtest/gtest.h"

namespace dbgroup::atomic::pmwcas::test
{
class DescriptorPoolFixture : public ::testing::Test
{
 protected:
  void
  SetUp() override
  {
    // 各テスト毎に実行する必要があるセットアップコードがある場合はここに記述するらしい
  }

  void
  TearDown() override
  {
    // 各テスト毎に実行する必要がある終了コードがある場合はここに記述するらしい
  }

  void
  ExecuteInOneThread()
  {
    PMwCASDescriptor *desc_1 = GetOneDescriptor();
    PMwCASDescriptor *desc_2 = GetOneDescriptor();
    EXPECT_EQ(desc_1, desc_2);
  }

  void
  ExecuteInMultipleThread(const size_t pool_size)
  {
    GetAllDescriptor(pool_size);

    // 重複を削除
    std::unordered_set<PMwCASDescriptor *> distinct(std::begin(desc_arr_), std::end(desc_arr_));
    EXPECT_EQ(distinct.size(), pool_size);
  }

 private:
  auto
  GetOneDescriptor() -> PMwCASDescriptor *
  {
    PMwCASDescriptor *desc = pool_.Get();
    return desc;
  }

  void
  GetAllDescriptor(const size_t pool_size)
  {
    std::vector<std::thread> threads;

    for (size_t i = 0; i < pool_size; ++i) {
      threads.emplace_back([&, i]() mutable {
        std::shared_lock guard{s_mtx_};
        PMwCASDescriptor *desc = GetOneDescriptor();
        desc_arr_[i] = desc;
        {
          std::unique_lock lock{x_mtx_};
          guard.unlock();
          cond_.wait(lock, [this] { return is_ready_; });
        }
      });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    [[maybe_unused]] std::lock_guard s_guard{s_mtx_};

    {
      std::lock_guard x_guard{x_mtx_};
      is_ready_ = true;
    }
    cond_.notify_all();

    for (auto &t : threads) {
      t.join();
    }
  }

  DescriptorPool pool_{};

  std::array<PMwCASDescriptor *, DESCRIPTOR_POOL_SIZE> desc_arr_{};

  std::mutex x_mtx_{};

  std::shared_mutex s_mtx_{};

  std::condition_variable cond_{};

  bool is_ready_{false};
};

TEST_F(DescriptorPoolFixture, GetTwoSameDescriptorInOneThread)
{  //
  ExecuteInOneThread();
}

TEST_F(DescriptorPoolFixture, GetDifferentDescriptorsInEveryThread)
{  //
  auto pool_size = DESCRIPTOR_POOL_SIZE;
  ExecuteInMultipleThread(pool_size);
}

}  // namespace dbgroup::atomic::pmwcas::test
