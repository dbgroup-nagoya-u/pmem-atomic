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
    PMwCASDescriptor *desc_1 = GetOneDescriptor();
    PMwCASDescriptor *desc_2 = GetOneDescriptor();
    // std::cout << desc_1 << std::endl;
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
      threads.emplace_back([&, i] {
        PMwCASDescriptor *desc = GetOneDescriptor();
        desc_arr_[i] = desc;
        std::cout << desc << std::endl;
      });
    }

    for (auto &t : threads) {
      t.join();
    }
  }

  DescriptorPool pool_{};

  std::array<PMwCASDescriptor *, DESCRIPTOR_POOL_SIZE> desc_arr_{};
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
