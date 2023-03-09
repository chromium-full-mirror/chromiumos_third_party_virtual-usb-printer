// Copyright 2021 The ChromiumOS Authors
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
  auto request =
      std::make_unique<ipp::Frame>(ipp::Operation::Get_Printer_Attributes);

  // Fills in a bunch of string values.
  request->Groups(ipp::GroupTag::operation_attributes)
      .begin()
      ->AddAttr("printer-uri", ipp::ValueTag::uri, "ipps://hello-there");

  // Adds some extra unknown attributes of different types to exercise
  // some other codepaths outside of the string conversions.
  ipp::CollsView::iterator collection;
  request->AddGroup(ipp::GroupTag::unsupported_attributes, collection);

  collection->AddAttr("home on the", ipp::RangeOfInteger(-9, 9001));
  collection->AddAttr("hark! a boolean", true);
  collection->AddAttr("set of keywords", ipp::ValueTag::keyword,
                      {"one", "two", "three"});

  const MultilineLogHolder holder = ToString(*request);
  EXPECT_EQ(base::JoinString(holder.lines, "\n"),
            R"(Request (operation ID: 0x000b; groups: 2)
  Group (tag: 0x0001; collections: 1)
    Collection 0 (3 attributes)
      Attribute ``attributes-charset'' (charset, 1 values)
        utf-8
      Attribute ``attributes-natural-language'' (naturalLanguage, 1 values)
        en-us
      Attribute ``printer-uri'' (uri, 1 values)
        ipps://hello-there
  Group (tag: 0x0005; collections: 1)
    Collection 0 (3 attributes)
      Attribute ``home on the'' (rangeOfInteger, 1 values)
        (-9:9001)
      Attribute ``hark! a boolean'' (boolean, 1 values)
        true
      Attribute ``set of keywords'' (keyword, 3 values)
        one
        two
        three)");
}

}  // namespace
}  // namespace mock_printer
