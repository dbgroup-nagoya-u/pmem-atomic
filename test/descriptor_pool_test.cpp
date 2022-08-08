#include "pmwcas/descriptor_pool.hpp"

#include <iterator>
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
    PMwCASDescriptor *desc_1 = pool_.Get();
    PMwCASDescriptor *desc_2 = pool_.Get();
    EXPECT_EQ(desc_1, desc_2);
  }

  void
  ExecuteInMultipleThread()
  {
    std::vector<std::thread> threads;

    for (size_t i = 0; i < kThreadNum; ++i) {
      threads.emplace_back([&, i] {
        PMwCASDescriptor *desc = pool_.Get();
        desc_arr_[i] = desc;
      });
    }

    for (auto &t : threads) {
      t.join();
    }

    // 重複を削除
    std::unordered_set<PMwCASDescriptor *> distinct(std::begin(desc_arr_), std::end(desc_arr_));

    EXPECT_EQ(distinct.size(), kThreadNum);
  }

 private:
  DescriptorPool pool_{};

  std::array<PMwCASDescriptor *, kThreadNum> desc_arr_{};
};

TEST_F(DescriptorPoolFixture, GetTwoSameDescriptorInOneThread)
{  //
  ExecuteInOneThread();
}

TEST_F(DescriptorPoolFixture, GetDifferentDescriptorsInEveryThread)
{  //
  ExecuteInMultipleThread();
}

}  // namespace dbgroup::atomic::pmwcas::test
