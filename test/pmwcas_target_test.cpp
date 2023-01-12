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
#include "pmwcas/component/pmwcas_target.hpp"

// C++ standard libraries
#include <filesystem>

// external system libraries
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

// external libraries
#include "gtest/gtest.h"

// local sources
#include "common.hpp"

namespace dbgroup::atomic::pmwcas::component::test
{
/*######################################################################################
 * Global constants
 *####################################################################################*/

constexpr std::string_view kTmpPMEMPath = DBGROUP_ADD_QUOTES(DBGROUP_TEST_TMP_PMEM_PATH);
constexpr const char *kPoolName = "pmwcas_pmwcas_target_test";
constexpr const char *kLayout = "target";
constexpr auto kModeRW = S_IRUSR | S_IWUSR;  // NOLINT

template <class Target>
class PMwCASTargetFixture : public ::testing::Test
{
 protected:
  /*####################################################################################
   * Internal classes
   *##################################################################################*/

  struct PMEMRoot {
    ::pmem::obj::p<Target> target{};
  };

  /*####################################################################################
   * Setup/Teardown
   *##################################################################################*/

  void
  SetUp() override
  {
    if constexpr (std::is_same_v<Target, uint64_t *>) {
      old_val_ = new uint64_t{1};
      new_val_ = new uint64_t{2};
    } else {
      old_val_ = 1;
      new_val_ = 2;
    }

    try {
      // create a user directory for testing
      const std::string user_name{std::getenv("USER")};
      std::filesystem::path pool_path{kTmpPMEMPath};
      pool_path /= user_name;
      std::filesystem::create_directories(pool_path);
      pool_path /= kPoolName;
      std::filesystem::remove(pool_path);

      // create a persistent pool for testing
      constexpr size_t kSize = PMEMOBJ_MIN_POOL * 2;
      pool_ = ::pmem::obj::pool<PMEMRoot>::create(pool_path, kLayout, kSize, kModeRW);

      // allocate regions on persistent memory
      ::pmem::obj::transaction::run(pool_, [&] {
        auto &&root = pool_.root();
        root->target = old_val_;
      });
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
      std::terminate();
    }

    auto *addr = &(pool_.root()->target);
    pmwcas_target_ = PMwCASTarget{addr, old_val_, new_val_, std::memory_order_relaxed};
    desc_ = PMwCASField{0UL, true};
  }

  void
  TearDown() override
  {
    if constexpr (std::is_same_v<Target, uint64_t *>) {
      delete old_val_;
      delete new_val_;
    }

    pool_.close();
  }

  /*####################################################################################
   * Functions for verification
   *##################################################################################*/

  void
  VerifyEmbedDescriptor(const bool expect_fail)
  {
    auto &target = pool_.root()->target.get_rw();
    if (expect_fail) {
      target = new_val_;
    }

    const bool result = pmwcas_target_.EmbedDescriptor(desc_);

    if (expect_fail) {
      EXPECT_FALSE(result);
      EXPECT_NE(CASTargetConverter<PMwCASField>{desc_}.converted_data,  // NOLINT
                CASTargetConverter<Target>{target}.converted_data);     // NOLINT
    } else {
      EXPECT_TRUE(result);
      EXPECT_EQ(CASTargetConverter<PMwCASField>{desc_}.converted_data,  // NOLINT
                CASTargetConverter<Target>{target}.converted_data);     // NOLINT
    }
  }

  void
  VerifyCompletePMwCAS(const bool succeeded)
  {
    ASSERT_TRUE(pmwcas_target_.EmbedDescriptor(desc_));

    auto &target = pool_.root()->target.get_rw();
    if (succeeded) {
      pmwcas_target_.RedoPMwCAS();
      EXPECT_EQ(new_val_, target);
    } else {
      pmwcas_target_.UndoPMwCAS();
      EXPECT_EQ(old_val_, target);
    }
  }

  void
  VerifyRecoverPMwCAS(const bool succeeded)
  {
    ASSERT_TRUE(pmwcas_target_.EmbedDescriptor(desc_));

    auto &target = pool_.root()->target.get_rw();
    pmwcas_target_.Recover(succeeded, desc_);

    if (succeeded) {
      EXPECT_EQ(new_val_, target);
    } else {
      EXPECT_EQ(old_val_, target);
    }
  }

 private:
  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  ::pmem::obj::pool<PMEMRoot> pool_{};

  PMwCASTarget pmwcas_target_{};
  PMwCASField desc_{};

  Target old_val_{};
  Target new_val_{};
};

/*######################################################################################
 * Preparation for typed testing
 *####################################################################################*/

using Targets = ::testing::Types<uint64_t, uint64_t *, MyClass>;
TYPED_TEST_SUITE(PMwCASTargetFixture, Targets);

/*######################################################################################
 * Unit test definitions
 *####################################################################################*/

TYPED_TEST(PMwCASTargetFixture, EmbedDescriptorWithExpectedValueSucceedEmbedding)
{
  TestFixture::VerifyEmbedDescriptor(false);
}

TYPED_TEST(PMwCASTargetFixture, EmbedDescriptorWithUnexpectedValueFailEmbedding)
{
  TestFixture::VerifyEmbedDescriptor(true);
}

TYPED_TEST(PMwCASTargetFixture, CompletePMwCASWithSucceededStatusUpdateToDesiredValue)
{
  TestFixture::VerifyCompletePMwCAS(true);
}

TYPED_TEST(PMwCASTargetFixture, CompletePMwCASWithFailedStatusRevertToExpectedValue)
{
  TestFixture::VerifyCompletePMwCAS(false);
}

TYPED_TEST(PMwCASTargetFixture, RecoverPMwCASWithSucceededStatusRollForwardToDesiredValue)
{
  TestFixture::VerifyRecoverPMwCAS(false);
}

TYPED_TEST(PMwCASTargetFixture, RecoverPMwCASWithFailedStatusRollBackToExpectedValue)
{
  TestFixture::VerifyRecoverPMwCAS(true);
}
}  // namespace dbgroup::atomic::pmwcas::component::test
