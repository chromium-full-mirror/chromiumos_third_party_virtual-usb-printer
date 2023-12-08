// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_matching.h"

#include <memory>
#include <utility>

#include <base/check.h>
#include <chromeos/libipp/attribute.h>
#include <chromeos/libipp/frame.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "parse_textproto.h"
#include "mock_printer/proto/ipp.pb.h"

// Note that these unit tests do not exercise cases where the matching
// Protobufs are malformed. We assume that callers will have validated
// the Protobufs (see `ipp_matching_validation.h`) before passing them
// here.

namespace mock_printer {
namespace {

// Creates a canned `get-printer-attributes` request.
std::unique_ptr<ipp::Frame> GetPrinterAttributesRequest() {
  auto request =
      std::make_unique<ipp::Frame>(ipp::Operation::Get_Printer_Attributes);

  auto group = request->Groups(ipp::GroupTag::operation_attributes).begin();
  group->AddAttr("printer-uri", ipp::ValueTag::uri, "ipps://hello-there");

  return request;
}

// Holds an IPP request
// *  with one group,
// *  with one `collection`, and
// *  with no attributes.
//
// The `collection` pointer into `request` is provided to allow easy
// access to attribute manipulation.
struct RequestAndCollection {
  std::unique_ptr<ipp::Frame> request;
  ipp::CollsView::iterator collection;
};

// Creates an IPP request
// *  with a "shutdown one printer" op-code and
// *  with a `job_attributes` group.
//
// Caller is expected to populate one or more attributes.
RequestAndCollection RequestWithOneGroup() {
  auto libipp_request = std::make_unique<ipp::Frame>();
  libipp_request->OperationIdOrStatusCode() =
      static_cast<uint16_t>(ipp::Operation::Shutdown_One_Printer);
  auto holder = RequestAndCollection{.request = std::move(libipp_request)};
  holder.request->AddGroup(ipp::GroupTag::job_attributes, holder.collection);
  return holder;
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
        attributes {
          name: "attributes-charset"
          type: CHARSET
          string_values { string_values { value: "utf-8" } }
        }
        attributes {
          name: "attributes-natural-language"
          type: NATURAL_LANGUAGE
          string_values { string_values { value: "en-us" } }
        }
        attributes {
          name: "printer-uri"
          type: URI
          string_values { string_values { value: "ipps://hello-there" } }
        }
      }
    }
  )pb");

  EXPECT_TRUE(IppMessageMatches(*request, mock_request));
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
      }
    }
  )pb");

  EXPECT_TRUE(IppMessageMatches(*request, mock_request));
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
        attributes { name: "EXTRA-UNEXPECTED-ATTRIBUTE" }
      }
    }
  )pb");

  EXPECT_FALSE(IppMessageMatches(*request, mock_request));
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
        # the "printer-uri" attribute would go here.
      }
    }
  )pb");

  EXPECT_FALSE(IppMessageMatches(*request, mock_request));
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
        attributes { name: "attributes-natural-language" }
        attributes { name: "attributes-charset" }
      }
    }
  )pb");

  EXPECT_TRUE(IppMessageMatches(*request, mock_request));
}

// The same trivial test as `GetPrinterAttributesMatches`, but
// *  without values at the attributes level,
// *  with the IPP attributes in the matcher Protobuf in a different
//    order than what's specified in the ipp::Request, and
// *  with a bogus type specified for the `printer-uri` attribute,
//    causing a type mismatch.
TEST(IppMatchingTest, MatchingAttributesIsTypeSensitive) {
  const auto request = GetPrinterAttributesRequest();
  const mocking::IppMessage mock_request = ParseIppMessageOrDie(R"pb(
    # E_operations_supported::Get_Printer_Attributes
    operation_id: 0x000b

    groups {
      tag: OPERATION_ATTRIBUTES
      collections {
        attributes {
          name: "printer-uri"
          # The `printer-uri` attribute should have type `URI`.
          # Having the mock Protobuf specify `INTEGER` will cause a
          # mismatch.
          type: INTEGER
        }
        attributes { name: "attributes-natural-language" }
        attributes { name: "attributes-charset" }
      }
    }
  )pb");

  EXPECT_FALSE(IppMessageMatches(*request, mock_request));
}

// Tests that an IPP DateTime attribute can be matched.
TEST(IppMatchingTest, BasicDateTimeMatching) {
  auto request = RequestWithOneGroup();
  request.collection->AddAttr("some-date-and-time", ipp::DateTime{
                                                        .year = 2001,
                                                        .month = 1,
                                                        .day = 31,
                                                        .hour = 13,
                                                        .minutes = 13,
                                                    });
  const mocking::IppMessage mock_request = ParseIppMessageOrDie(R"pb(
    # E_operations_supported::Shutdown_One_Printer
    operation_id: 0x0050

    groups {
      tag: JOB_ATTRIBUTES
      collections {
        attributes {
          name: "some-date-and-time"
          type: DATETIME
          date_times {
            date_times { year: 2001 month: 1 day: 31 hour: 13 minutes: 13 }
          }
        }
      }
    }
  )pb");

  EXPECT_TRUE(IppMessageMatches(*request.request, mock_request));
}

// Tests that an IPP DateTime attribute can be matched even when the
// mock Protobuf uses "wildcard" values.
TEST(IppMatchingTest, DateTimeMatchingWithWildcardMembers) {
  auto request = RequestWithOneGroup();
  request.collection->AddAttr("some-date-and-time", ipp::DateTime{
                                                        .year = 2001,
                                                        .month = 1,
                                                        .day = 31,
                                                        .hour = 13,
                                                        .minutes = 13,
                                                    });
  const mocking::IppMessage mock_request = ParseIppMessageOrDie(R"pb(
    # E_operations_supported::Shutdown_One_Printer
    operation_id: 0x0050

    groups {
      tag: JOB_ATTRIBUTES
      collections {
        attributes {
          name: "some-date-and-time"
          type: DATETIME
          # Intermingles zero-valued members together with wholly
          # unspecified fields (hour & minutes).
          date_times { date_times { year: 2001 month: 0 day: 0 } }
        }
      }
    }
  )pb");

  EXPECT_TRUE(IppMessageMatches(*request.request, mock_request));
}

}  // namespace
}  // namespace mock_printer
