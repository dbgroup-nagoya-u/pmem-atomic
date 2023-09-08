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

// local sources
#include "common.hpp"

namespace dbgroup::atomic::pmwcas::component::test
{
// prepare a temporary directory
auto *const env = testing::AddGlobalTestEnvironment(new TmpDirManager);

/*######################################################################################
 * Global constants
 *####################################################################################*/

constexpr const char *kPoolName = "pmwcas_pmwcas_target_test";
constexpr const char *kLayout = "target";
constexpr auto kModeRW = S_IRUSR | S_IWUSR;  // NOLINT

template <class Target>
class PMwCASTargetFixture : public ::testing::Test
{
 protected:
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

    // create a user directory for testing
    auto &&pool_path = GetTmpPoolPath();
    pool_path /= kPoolName;

    // create a persistent pool for testing
    constexpr size_t kPoolSize = PMEMOBJ_MIN_POOL;
    if (std::filesystem::exists(pool_path)) {
      pop_ = pmemobj_open(pool_path.c_str(), kLayout);
    } else {
      pop_ = pmemobj_create(pool_path.c_str(), kLayout, kPoolSize, kModeRW);
    }
    if (pmemobj_zalloc(pop_, &oid_, sizeof(Target), 0) != 0) {
      std::cerr << pmemobj_errormsg() << std::endl;
      throw std::exception{};
    }

    // prepare initial state
    target_ = reinterpret_cast<Target *>(pmemobj_direct(oid_));
    *target_ = old_val_;
    pmemobj_persist(pop_, target_, sizeof(Target));

    pmwcas_target_ = PMwCASTarget{target_, old_val_, new_val_, std::memory_order_relaxed};
    desc_ = PMwCASField{0UL, true};
  }

  void
  TearDown() override
  {
    if constexpr (std::is_same_v<Target, uint64_t *>) {
      delete old_val_;
      delete new_val_;
    }

    if (!OID_IS_NULL(oid_)) {
      pmemobj_free(&oid_);
    }

    if (pop_ != nullptr) {
      pmemobj_close(pop_);
    }
  }

  /*####################################################################################
   * Functions for verification
   *##################################################################################*/

  void
  VerifyEmbedDescriptor(const bool expect_fail)
  {
    if (expect_fail) {
      *target_ = new_val_;
    }

    const bool result = pmwcas_target_.EmbedDescriptor(desc_);

    if (expect_fail) {
      EXPECT_FALSE(result);
      EXPECT_NE(CASTargetConverter<PMwCASField>{desc_}.converted_data,  // NOLINT
                CASTargetConverter<Target>{*target_}.converted_data);   // NOLINT
    } else {
      EXPECT_TRUE(result);
      EXPECT_EQ(CASTargetConverter<PMwCASField>{desc_}.converted_data,  // NOLINT
                CASTargetConverter<Target>{*target_}.converted_data);   // NOLINT
    }
  }

  void
  VerifyCompletePMwCAS(const bool succeeded)
  {
    ASSERT_TRUE(pmwcas_target_.EmbedDescriptor(desc_));

    if (succeeded) {
      pmwcas_target_.Redo();
      EXPECT_EQ(new_val_, *target_);
    } else {
      pmwcas_target_.Undo();
      EXPECT_EQ(old_val_, *target_);
    }
  }

  void
  VerifyRecoverPMwCAS(const bool succeeded)
  {
    ASSERT_TRUE(pmwcas_target_.EmbedDescriptor(desc_));

    pmwcas_target_.Recover(succeeded, desc_);

    if (succeeded) {
      EXPECT_EQ(new_val_, *target_);
    } else {
      EXPECT_EQ(old_val_, *target_);
    }
  }

 private:
  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  PMEMobjpool *pop_{nullptr};
  PMEMoid oid_{OID_NULL};
  Target *target_{nullptr};

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
