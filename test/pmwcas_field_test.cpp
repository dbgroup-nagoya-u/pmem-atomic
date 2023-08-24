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

#include "pmwcas/component/pmwcas_field.hpp"

#include "common.hpp"
#include "gtest/gtest.h"

namespace dbgroup::atomic::pmwcas::component::test
{
template <class Target>
class PMwCASFieldFixture : public ::testing::Test
{
 protected:
  /*####################################################################################
   * Setup/Teardown
   *##################################################################################*/

  void
  SetUp() override
  {
    if constexpr (std::is_same_v<Target, uint64_t *>) {
      data_1_ = new uint64_t{1};
      data_2_ = new uint64_t{2};
    } else {
      data_1_ = 1;
      data_2_ = 2;
    }
  }

  void
  TearDown() override
  {
    if constexpr (std::is_same_v<Target, uint64_t *>) {
      delete data_1_;
      delete data_2_;
    }
  }

  /*####################################################################################
   * Functions for verification
   *##################################################################################*/

  void
  VerifyConstructor(const bool is_pmwcas_desc)
  {
    auto target_word_1 = PMwCASField{data_1_, is_pmwcas_desc};
    EXPECT_FALSE(target_word_1.IsNotPersisted());

    if (is_pmwcas_desc) {
      if constexpr (kUseDirtyFlag) {
        EXPECT_FALSE(target_word_1.IsPMwCASDescriptor());
        auto dirty_word = target_word_1;
        dirty_word.SetDirtyFlag(true);
        EXPECT_TRUE(dirty_word.IsPMwCASDescriptor());
      } else {
        EXPECT_TRUE(target_word_1.IsPMwCASDescriptor());
      }
    } else {
      EXPECT_FALSE(target_word_1.IsPMwCASDescriptor());
      if constexpr (kUseDirtyFlag) {
        auto dirty_word = target_word_1;
        dirty_word.SetDirtyFlag(true);
        EXPECT_TRUE(dirty_word.IsNotPersisted());
        auto persisted_word = dirty_word;
        persisted_word.SetDirtyFlag(false);
        EXPECT_FALSE(persisted_word.IsNotPersisted());
        EXPECT_EQ(target_word_1, persisted_word);
      }
    }
    EXPECT_EQ(data_1_, target_word_1.GetTargetData<Target>());
  }

  void
  VerifyEQ()
  {
    PMwCASField field_a{data_1_, false};
    PMwCASField field_b{data_1_, false};
    EXPECT_TRUE(field_a == field_b);

    if constexpr (kUseDirtyFlag) {
      field_b = PMwCASField{data_2_, false};
      EXPECT_FALSE(field_a == field_b);

      field_a = field_b;
      field_a.SetDirtyFlag(true);
      EXPECT_FALSE(field_a == field_b);

      field_b = PMwCASField{data_2_, true};
      EXPECT_FALSE(field_a == field_b);

      field_a.SetDirtyFlag(false);
      EXPECT_FALSE(field_a == field_b);

      field_b = PMwCASField{data_2_, false};
      EXPECT_TRUE(field_a == field_b);
    } else {
      field_b = PMwCASField{data_2_, false};
      EXPECT_FALSE(field_a == field_b);

      field_a = PMwCASField{data_2_, true};
      EXPECT_FALSE(field_a == field_b);

      field_b = PMwCASField{data_2_, true};
      EXPECT_TRUE(field_a == field_b);
    }
  }

  void
  VerifyNE()
  {
    PMwCASField field_a{data_1_, false};
    PMwCASField field_b{data_1_, false};
    EXPECT_FALSE(field_a != field_b);

    if constexpr (kUseDirtyFlag) {
      field_b = PMwCASField{data_2_, false};
      EXPECT_TRUE(field_a != field_b);

      field_a = field_b;
      field_a.SetDirtyFlag(true);
      EXPECT_TRUE(field_a != field_b);

      field_b = PMwCASField{data_2_, true};
      EXPECT_TRUE(field_a != field_b);

      field_a.SetDirtyFlag(false);
      EXPECT_TRUE(field_a != field_b);

      field_b = PMwCASField{data_2_, false};
      EXPECT_FALSE(field_a != field_b);
    } else {
      field_b = PMwCASField{data_2_, false};
      EXPECT_TRUE(field_a != field_b);

      field_a = PMwCASField{data_2_, true};
      EXPECT_TRUE(field_a != field_b);

      field_b = PMwCASField{data_2_, true};
      EXPECT_FALSE(field_a != field_b);
    }
  }

 private:
  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  Target data_1_{};
  Target data_2_{};
};

/*######################################################################################
 * Preparation for typed testing
 *####################################################################################*/

using Targets = ::testing::Types<uint64_t, uint64_t *, MyClass>;
TYPED_TEST_SUITE(PMwCASFieldFixture, Targets);

/*######################################################################################
 * Unit test definitions
 *####################################################################################*/

TYPED_TEST(PMwCASFieldFixture, ConstructorWithoutDescriptorFlagCreateValueField)
{
  TestFixture::VerifyConstructor(false);
}

TYPED_TEST(PMwCASFieldFixture, ConstructorWithDescriptorFlagCreateDescriptorField)
{
  TestFixture::VerifyConstructor(true);
}

TYPED_TEST(PMwCASFieldFixture, EQWithAllCombinationOfInstancesReturnCorrectBool)
{
  TestFixture::VerifyEQ();
}

TYPED_TEST(PMwCASFieldFixture, NEWithAllCombinationOfInstancesReturnCorrectBool)
{
  TestFixture::VerifyNE();
}

}  // namespace dbgroup::atomic::pmwcas::component::test
