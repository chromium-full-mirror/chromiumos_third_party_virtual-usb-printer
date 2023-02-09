// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_PARSE_TEXTPROTO_H_
#define MOCK_PRINTER_PARSE_TEXTPROTO_H_

#include <optional>
#include <string>

#include "mock_printer/proto/control_flow.pb.h"
#include "mock_printer/proto/http.pb.h"
#include "mock_printer/proto/ipp.pb.h"

namespace mock_printer {

// Returns a `mocking::TestCase` parsed from `textproto`. Returns
// std::nullopt if parsing fails.
std::optional<mocking::TestCase> ParseTestCase(const std::string& textproto);

// Returns a `mocking::TestCase` parsed from `textproto`. Aborts
// the program if parsing fails.
//
// This function is intended for use in unit tests where we don't ever
// expect a bogus `mocking::TestCase` at all.
mocking::TestCase ParseTestCaseOrDie(const std::string& textproto);

// Returns a `mocking::HttpProperties` parsed from `textproto`. Aborts the
// program if parsing fails.
//
// This function is intended for use in unit tests, since `virtual-usb-printer`
// doesn't need to parse `mocking::HttpProperties` in isolation.
mocking::HttpProperties ParseHttpPropertiesOrDie(const std::string& textproto);

// Returns a `mocking::IppMessage` parsed from `textproto`. Aborts
// the program if parsing fails.
//
// This function is intended for use in unit tests, since
// `virtual-usb-printer` doesn't need to parse `mocking::IppMessage`
// in isolation.
mocking::IppMessage ParseIppMessageOrDie(const std::string& textproto);

// Returns a `mocking::Cardinality` parsed from `textproto`. Aborts the
// program if parsing fails.
//
// This function is intended for use in unit tests, since
// `virtual-usb-printer` doesn't need to parse `mocking::Cardinality`
// in isolation.
mocking::Cardinality ParseCardinalityOrDie(const std::string& textproto);

}  // namespace mock_printer

#endif  // MOCK_PRINTER_PARSE_TEXTPROTO_H_
