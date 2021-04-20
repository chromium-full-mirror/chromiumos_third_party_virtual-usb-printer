// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_matching_validation.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "parse_textproto.h"
#include "proto/ipp.pb.h"

namespace mock_printer {
namespace {

using ::testing::HasSubstr;

TEST(IppMatchingValidation, IppMessageDisallowsZeroedOperationId) {
  const mocking::IppMessage message = ParseIppMessageOrDie(R"pb(
    # Operation ID must not be 0.
    operation_id: 0

    # This IppGroup is irrelevant to the test case.
    groups { tag: DOCUMENT_ATTRIBUTES }
  )pb");

  const ProtobufValidationResult result = ValidateIppMessage(message);
  ASSERT_FALSE(result.IsValid());
  EXPECT_THAT(result.error_message, HasSubstr("operation_id"));
}

TEST(IppMatchingValidation, IppMessageAllowsNonzeroOperationId) {
  const mocking::IppMessage message = ParseIppMessageOrDie(R"pb(
    operation_id: 1

    # This IppGroup is irrelevant to the test case.
    groups { tag: DOCUMENT_ATTRIBUTES }
  )pb");

  EXPECT_TRUE(ValidateIppMessage(message).IsValid());
}

TEST(IppMatchingValidation, IppMessageRequiresIppGroup) {
  const mocking::IppMessage message = ParseIppMessageOrDie(R"pb(
    operation_id: 1

    # This IppMessage lacks an IppGroup in its required `groups` field.
    # Parsing will succeed here, but validation will fail.
  )pb");

  const ProtobufValidationResult result = ValidateIppMessage(message);
  ASSERT_FALSE(result.IsValid());
  EXPECT_THAT(result.error_message, HasSubstr("IppGroup"));
}

TEST(IppMatchingValidation, IppGroupDisallowsUnspecifiedTag) {
  const mocking::IppMessage message = ParseIppMessageOrDie(R"pb(
    operation_id: 1

    # IppGroupTag::UNSPECIFIED is an illegal value here.
    # Parsing will succeed here, but validation will fail.
    groups { tag: UNSPECIFIED }
  )pb");

  const ProtobufValidationResult result = ValidateIppMessage(message);
  ASSERT_FALSE(result.IsValid());
  EXPECT_THAT(result.error_message, HasSubstr("IppGroupTag"));
}

TEST(IppMatchingValidation, IppCollectionMustContainAttributes) {
  const mocking::IppMessage message = ParseIppMessageOrDie(R"pb(
    operation_id: 1

    groups {
      # This IppGroupTag is irrelevant to the test case.
      tag: DOCUMENT_ATTRIBUTES

      # This IppCollection is empty, which is an illegal construct.
      # Parsing will succeed here, but validation will fail.
      collections {}
    }
  )pb");

  const ProtobufValidationResult result = ValidateIppMessage(message);
  ASSERT_FALSE(result.IsValid());
  EXPECT_THAT(result.error_message, HasSubstr("empty IppCollection"));
}

TEST(IppMatchingValidation, IppAttributeMustBeNamed) {
  const mocking::IppMessage message = ParseIppMessageOrDie(R"pb(
    operation_id: 1

    groups {
      # This IppGroupTag is irrelevant to the test case.
      tag: DOCUMENT_ATTRIBUTES

      collections {
        # This IppAttribute has no name.
        # Parsing will succeed here, but validation will fail.
        attributes {}
      }
    }
  )pb");

  const ProtobufValidationResult result = ValidateIppMessage(message);
  ASSERT_FALSE(result.IsValid());
  EXPECT_THAT(result.error_message, HasSubstr("empty name"));
}

TEST(IppMatchingValidation, IppAttributeNeedsOnlyName) {
  const mocking::IppMessage message = ParseIppMessageOrDie(R"pb(
    operation_id: 1

    groups {
      # This IppGroupTag is irrelevant to the test case.
      tag: DOCUMENT_ATTRIBUTES

      collections {
        # This IppAttribute has a name and no other fields filled in.
        # This is a legal construct.
        attributes { name: "foo" }
      }
    }
  )pb");

  EXPECT_TRUE(ValidateIppMessage(message).IsValid());
}

TEST(IppMatchingValidation, IppAttributeMayContainAtMostOneSubmessage) {
  const mocking::IppMessage message = ParseIppMessageOrDie(R"pb(
    operation_id: 1

    groups {
      # This IppGroupTag is irrelevant to the test case.
      tag: DOCUMENT_ATTRIBUTES

      collections {
        attributes {
          name: "hello there"

          # This IppAttribute has more than one value field populated;
          # this is an illegal construct. Parsing will succeed here,
          # but validation will fail.
          int_values: 1
          int_values: 2
          int_values: 3
          string_values: "general kenobi"
        }
      }
    }
  )pb");

  const ProtobufValidationResult result = ValidateIppMessage(message);
  ASSERT_FALSE(result.IsValid());
  EXPECT_THAT(result.error_message, HasSubstr("multiple populated"));
}

TEST(IppMatchingValidation, IppAttributeWithCollectionIsRecursivelyValidated) {
  const mocking::IppMessage message = ParseIppMessageOrDie(R"pb(
    operation_id: 1

    groups {
      # This IppGroupTag is irrelevant to the test case.
      tag: DOCUMENT_ATTRIBUTES

      collections {
        attributes {
          name: "hello there"

          # This IppCollection is placed in a valid spot; the containing
          # IppAttribute is well-formed, but this empty IppCollection is
          # illegal and will fail validation.
          collections {}
        }
      }
    }
  )pb");

  const ProtobufValidationResult result = ValidateIppMessage(message);
  ASSERT_FALSE(result.IsValid());
  EXPECT_THAT(result.error_message, HasSubstr("empty IppCollection"));
}

TEST(IppMatchingValidation, IppAttributeDisallowsUnspecifiedResolutionUnits) {
  const mocking::IppMessage message = ParseIppMessageOrDie(R"pb(
    operation_id: 1

    groups {
      # This IppGroupTag is irrelevant to the test case.
      tag: DOCUMENT_ATTRIBUTES

      collections {
        attributes {
          name: "hello there"

          resolutions {
            # This Units variant is illegal and will fail validation.
            units: UNSPECIFIED
          }
        }
      }
    }
  )pb");

  const ProtobufValidationResult result = ValidateIppMessage(message);
  ASSERT_FALSE(result.IsValid());
  EXPECT_THAT(result.error_message, HasSubstr("Units"));
}

}  // namespace
}  // namespace mock_printer
