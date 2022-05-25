// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "proto_to_libipp.h"

#include <memory>
#include <string>

#include <chromeos/libipp/attribute.h>
#include <chromeos/libipp/frame.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "parse_textproto.h"

namespace mock_printer {
namespace {

using ::testing::NotNull;

TEST(ProtoToLibippTest, BasicConversionTest) {
  const mocking::IppMessage mock_message = ParseIppMessageOrDie(R"pb(
    # ipp::Operation::Get_Printer_Attributes
    operation_id: 0x000b

    # ipp::Status::successful_ok_events_complete
    status: 0x0007

    groups {
      tag: OPERATION_ATTRIBUTES
      collections {
        attributes {
          name: "attributes-charset"
          type: CHARSET
          string_values { string_values { value: "utf-8" } }
        }
      }
    }
  )pb");
  std::unique_ptr<ipp::Frame> response = ToResponse(mock_message);

  ASSERT_THAT(response, NotNull());

  // Examine the response at the top level.
  EXPECT_EQ(response->StatusCode(), ipp::Status::successful_ok_events_complete);

  for (ipp::GroupTag tag : ipp::kGroupTags) {
    if (tag == ipp::GroupTag::operation_attributes) {
      ASSERT_GE(response->GetGroups(tag).size(), 1ull);
    }
  }

  // Unpack the IPP group [of interest].
  const ipp::Collection* group =
      response->GetGroup(ipp::GroupTag::operation_attributes);
  ASSERT_THAT(group, NotNull());
  ASSERT_GE(group->GetAllAttributes().size(), 1ull);

  // Unpack the IPP attribute [of interest].
  const ipp::Attribute* attribute = group->GetAttribute("attributes-charset");
  ASSERT_THAT(attribute, NotNull());

  // Examine the IPP attribute.
  EXPECT_EQ(attribute->Tag(), ipp::ValueTag::charset);
  ASSERT_EQ(attribute->Size(), 1ull);

  std::string attribute_value;
  EXPECT_TRUE(attribute->GetValue(&attribute_value, 0ull));
  EXPECT_EQ(attribute_value, "utf-8");
}

}  // namespace
}  // namespace mock_printer
