// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_matching_validation.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "proto_test_utils.h"
#include "proto/ipp.pb.h"

namespace mock_printer {
namespace {

using ::testing::HasSubstr;

TEST(IppMatchingValidation, IppMessageDisallowsZeroedOperationId) {
  const mocking::IppMessage message = ParseTestProto(R"(
    # Operation ID must not be 0.
    operation_id: 0

    # This IppGroup is irrelevant to the test case.
    groups { tag: DOCUMENT_ATTRIBUTES }
  )");

  const ProtobufValidationResult result = ValidateIppMessage(message);
  ASSERT_FALSE(result.IsValid());
  EXPECT_THAT(result.error_message, HasSubstr("operation_id"));
}

TEST(IppMatchingValidation, IppMessageAllowsNonzeroOperationId) {
  const mocking::IppMessage message = ParseTestProto(R"(
    operation_id: 1

    # This IppGroup is irrelevant to the test case.
    groups { tag: DOCUMENT_ATTRIBUTES }
  )");

  EXPECT_TRUE(ValidateIppMessage(message).IsValid());
}

TEST(IppMatchingValidation, IppMessageRequiresIppGroup) {
  const mocking::IppMessage message = ParseTestProto(R"(
    operation_id: 1

    # This IppMessage lacks an IppGroup in its required `groups` field.
    # Parsing will succeed here, but validation will fail.
  )");

  const ProtobufValidationResult result = ValidateIppMessage(message);
  ASSERT_FALSE(result.IsValid());
  EXPECT_THAT(result.error_message, HasSubstr("IppGroup"));
}

TEST(IppMatchingValidation, IppGroupDisallowsUnspecifiedTag) {
  const mocking::IppMessage message = ParseTestProto(R"(
    operation_id: 1

    # IppGroupTag::UNSPECIFIED is an illegal value here.
    # Parsing will succeed here, but validation will fail.
    groups { tag: UNSPECIFIED }
  )");

  const ProtobufValidationResult result = ValidateIppMessage(message);
  ASSERT_FALSE(result.IsValid());
  EXPECT_THAT(result.error_message, HasSubstr("IppGroupTag"));
}

}  // namespace
}  // namespace mock_printer
