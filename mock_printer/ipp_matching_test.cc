// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_matching.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "chromeos/libipp/ipp.h"
#include "parse_textproto.h"
#include "proto/ipp.pb.h"

// Note that these unit tests do not exercise cases where the matching
// Protobufs are malformed. We assume that callers will have validated
// the Protobufs (see `ipp_matching_validation.h`) before passing them
// here.

namespace mock_printer {
namespace {

std::unique_ptr<ipp::Request> GetPrinterAttributesRequest() {
  auto request = std::make_unique<ipp::Request_Get_Printer_Attributes>();

  request->operation_attributes->attributes_charset.Set("utf-8");
  request->operation_attributes->attributes_natural_language.Set("en-US");
  request->operation_attributes->printer_uri.Set("ipps://hello-there");
  request->operation_attributes->requesting_user_name.Set("general kenobi");
  request->operation_attributes->requested_attributes.Set(
      {"one", "two", "three"});
  request->operation_attributes->document_format.Set("PWG Raster, maybe");
  return request;
}

// Trivial test written to show what the standard get-printer-attributes
// returned by libipp looks like.
TEST(IppMatchingTest, GetPrinterAttributesMatches) {
  const auto request = GetPrinterAttributesRequest();
  const mocking::IppMessage mock_request = ParseIppMessageOrDie(R"pb(
    # E_operations_supported::Get_Printer_Attributes
    operation_id: 0x000b

    groups {
      tag: OPERATION_ATTRIBUTES
      collections {
        attributes { name: "attributes-charset" string_values: "utf-8" }
        attributes {
          name: "attributes-natural-language"
          string_values: "en-US"
        }
        attributes { name: "printer-uri" string_values: "ipps://hello-there" }
        attributes {
          name: "requesting-user-name"
          string_values: "general kenobi"
        }
        attributes {
          name: "requested-attributes"
          string_values: "one"
          string_values: "two"
          string_values: "three"
        }
        attributes {
          name: "document-format"
          string_values: "PWG Raster, maybe"
        }
      }
    }
  )pb");

  EXPECT_TRUE(IppMessageMatches(request.get(), mock_request));
}

// The same trivial test as `GetPrinterAttributesMatches`, but without
// _values_ at the attributes level. Demonstrates that matching can
// depend solely on attribute names (and ignore values if not specified
// in the matcher Protobufs).
TEST(IppMatchingTest, MatchingNeedNotDependOnAttributeValues) {
  const auto request = GetPrinterAttributesRequest();
  const mocking::IppMessage mock_request = ParseIppMessageOrDie(R"pb(
    # E_operations_supported::Get_Printer_Attributes
    operation_id: 0x000b

    groups {
      tag: OPERATION_ATTRIBUTES
      collections {
        attributes { name: "attributes-charset" }
        attributes { name: "attributes-natural-language" }
        attributes { name: "printer-uri" }
        attributes { name: "requesting-user-name" }
        attributes { name: "requested-attributes" }
        attributes { name: "document-format" }
      }
    }
  )pb");

  EXPECT_TRUE(IppMessageMatches(request.get(), mock_request));
}

// The same trivial test as `GetPrinterAttributesMatches`, but
// *  without values at the attributes level and
// *  with an extra IPP attribute, which will cause matching to fail.
TEST(IppMatchingTest, MatchingForbidsExtraIppAttributes) {
  const auto request = GetPrinterAttributesRequest();
  const mocking::IppMessage mock_request = ParseIppMessageOrDie(R"pb(
    # E_operations_supported::Get_Printer_Attributes
    operation_id: 0x000b

    groups {
      tag: OPERATION_ATTRIBUTES
      collections {
        attributes { name: "attributes-charset" }
        attributes { name: "attributes-natural-language" }
        attributes { name: "printer-uri" }
        attributes { name: "requesting-user-name" }
        attributes { name: "requested-attributes" }
        attributes { name: "document-format" }
        attributes { name: "EXTRA-UNEXPECTED-ATTRIBUTE" }
      }
    }
  )pb");

  EXPECT_FALSE(IppMessageMatches(request.get(), mock_request));
}

// The same trivial test as `GetPrinterAttributesMatches`, but
// *  without values at the attributes level and
// *  with one less IPP attribute than the ipp::Request, which will
//    cause matching to fail.
TEST(IppMatchingTest, MatchingRequiresAllIppAttributes) {
  const auto request = GetPrinterAttributesRequest();
  const mocking::IppMessage mock_request = ParseIppMessageOrDie(R"pb(
    # E_operations_supported::Get_Printer_Attributes
    operation_id: 0x000b

    groups {
      tag: OPERATION_ATTRIBUTES
      collections {
        attributes { name: "attributes-charset" }
        attributes { name: "attributes-natural-language" }
        attributes { name: "printer-uri" }
        attributes { name: "requesting-user-name" }
        attributes { name: "requested-attributes" }
        # the "document-format" attribute would go here.
      }
    }
  )pb");

  EXPECT_FALSE(IppMessageMatches(request.get(), mock_request));
}

// The same trivial test as `GetPrinterAttributesMatches`, but
// *  without values at the attributes level and
// *  with the IPP attributes in the matcher Protobuf in a different
//    order than what's specified in the ipp::Request.
TEST(IppMatchingTest, MatchingDoesNotRequireOrderedIppAttributes) {
  const auto request = GetPrinterAttributesRequest();
  const mocking::IppMessage mock_request = ParseIppMessageOrDie(R"pb(
    # E_operations_supported::Get_Printer_Attributes
    operation_id: 0x000b

    groups {
      tag: OPERATION_ATTRIBUTES
      collections {
        attributes { name: "printer-uri" }
        attributes { name: "requesting-user-name" }
        attributes { name: "attributes-natural-language" }
        attributes { name: "document-format" }
        attributes { name: "attributes-charset" }
        attributes { name: "requested-attributes" }
      }
    }
  )pb");

  EXPECT_TRUE(IppMessageMatches(request.get(), mock_request));
}

}  // namespace
}  // namespace mock_printer
