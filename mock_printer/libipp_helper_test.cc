// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libipp_helper.h"

#include <memory>
#include <utility>

#include <base/strings/string_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mock_printer {
namespace {

// Tests `ToString()` for basic functionality.
TEST(ToStringTest, BasicGetPrinterAttributes) {
  // Creates a canned `get-printer-attributes` request.
  auto request = std::make_unique<ipp::Request_Get_Printer_Attributes>();

  // Fills in a bunch of string values.
  request->operation_attributes->attributes_charset.Set("utf-8");
  request->operation_attributes->attributes_natural_language.Set("en-US");
  request->operation_attributes->printer_uri.Set("ipps://hello-there");
  request->operation_attributes->requesting_user_name.Set(
      ipp::StringWithLanguage("general kenobi", "en-US"));
  request->operation_attributes->requested_attributes.Set(
      {"one", "two", "three"});
  request->operation_attributes->document_format.Set("PWG Raster, maybe");

  // Adds some extra unknown attributes of different types to exercise
  // some other codepaths outside of the string conversions.
  ipp::Group* group = request->AddUnknownGroup(
      ipp::GroupTag::unsupported_attributes, /*is_a_set=*/false);
  group->Resize(1);
  ipp::Collection* collection = group->GetCollection(0);
  ipp::Attribute* boolean_attribute = collection->AddUnknownAttribute(
      "hark! a boolean", /*is_a_set=*/false, ipp::AttrType::boolean);
  boolean_attribute->SetValue(1, 0);
  ipp::Attribute* range_attribute = collection->AddUnknownAttribute(
      "home on the", /*is_a_set=*/false, ipp::AttrType::rangeOfInteger);
  range_attribute->SetValue(ipp::RangeOfInteger(-9, 9001), 0);

  const std::vector<std::string> lines = ToString(request.get());
  EXPECT_EQ(base::JoinString(lines, "\n"),
            R"(Request (operation ID: 0x000b; groups: 2)
  Group (tag: 0x0001; collections: 1)
    Collection 0 (6 attributes)
      Attribute ``attributes-charset'' (charset, 1 values)
        utf-8
      Attribute ``attributes-natural-language'' (naturalLanguage, 1 values)
        en-US
      Attribute ``printer-uri'' (uri, 1 values)
        ipps://hello-there
      Attribute ``requesting-user-name'' (name, 1 values)
        general kenobi
      Attribute ``requested-attributes'' (keyword, 3 values)
        one
        two
        three
      Attribute ``document-format'' (mimeMediaType, 1 values)
        PWG Raster, maybe
  Group (tag: 0x0005; collections: 1)
    Collection 0 (2 attributes)
      Attribute ``home on the'' (rangeOfInteger, 1 values)
        (-9:9001)
      Attribute ``hark! a boolean'' (boolean, 1 values)
        true)");
}

}  // namespace
}  // namespace mock_printer
