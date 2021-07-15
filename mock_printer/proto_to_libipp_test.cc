// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "proto_to_libipp.h"

#include <memory>
#include <string>

#include <chromeos/libipp/ipp.h>
#include <chromeos/libipp/ipp_attribute.h>
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
  std::unique_ptr<ipp::Response> response = ToResponse(mock_message);

  ASSERT_THAT(response, NotNull());

  // Examine the response at the top level.
  EXPECT_EQ(response->GetOperationId(), ipp::Operation::Get_Printer_Attributes);
  EXPECT_EQ(response->StatusCode(), ipp::Status::successful_ok_events_complete);

  // As a practical matter, `libipp` will have populated some other
  // groups as per the defined schema for `get-printer-attributes`.
  ASSERT_GE(response->GetAllGroups().size(), 1ull);

  // Unpack the IPP group [of interest].
  const ipp::Group* group =
      response->GetGroup(ipp::GroupTag::operation_attributes);
  ASSERT_THAT(group, NotNull());

  ASSERT_EQ(group->GetSize(), 1ull);

  // Unpack the IPP collection.
  const ipp::Collection* collection = group->GetCollection(0ull);
  ASSERT_THAT(collection, NotNull());

  // As a practical matter, `libipp` populates some other attributes
  // as per the defined schema for a group of this type.
  ASSERT_GE(collection->GetAllAttributes().size(), 1ull);

  // Unpack the IPP attribute [of interest].
  const ipp::Attribute* attribute =
      collection->GetAttribute("attributes-charset");
  ASSERT_THAT(attribute, NotNull());

  // Examine the IPP attribute.
  EXPECT_EQ(attribute->GetType(), ipp::AttrType::charset);
  EXPECT_FALSE(attribute->IsASet());
  ASSERT_EQ(attribute->GetSize(), 1ull);

  std::string attribute_value;
  EXPECT_TRUE(attribute->GetValue(&attribute_value, 0ull));
  EXPECT_EQ(attribute_value, "utf-8");
}

}  // namespace
}  // namespace mock_printer
