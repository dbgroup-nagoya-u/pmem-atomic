#include "pmwcas/descriptor_pool.hpp"

#include "common.hpp"
#include "gtest/gtest.h"

namespace dbgroup::atomic::pmwcas::test
{

TEST(DescriptorPoolFixture, Check)
{
  DescriptorPool pool{};
  PMwCASDescriptor *descp = pool.Get();
}

}  // namespace dbgroup::atomic::pmwcas::test
