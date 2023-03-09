// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "proto_to_libipp.h"

#include <memory>
#include <string>
#include <vector>

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
      ASSERT_GE(response->Groups(tag).size(), 1ull);
    }
  }

  // Unpack the IPP group [of interest].
  ipp::CollsView::const_iterator group;
  group = response->Groups(ipp::GroupTag::operation_attributes).begin();
  ASSERT_GE(group->size(), 1ull);

  // Unpack the IPP attribute [of interest].
  ipp::Collection::const_iterator attribute =
      group->GetAttr("attributes-charset");
  ASSERT_NE(attribute, group->end());

  // Examine the IPP attribute.
  EXPECT_EQ(attribute->Tag(), ipp::ValueTag::charset);
  ASSERT_EQ(attribute->Size(), 1ull);

  std::vector<std::string> attribute_value;
  EXPECT_EQ(attribute->GetValues(attribute_value), ipp::Code::kOK);
  EXPECT_EQ(attribute_value, std::vector<std::string>{"utf-8"});
}

}  // namespace
}  // namespace mock_printer
