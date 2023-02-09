// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "parse_textproto.h"

#include <optional>
#include <string>
#include <utility>

#include <google/protobuf/text_format.h>

#include <base/check.h>
#include <base/logging.h>

#include "mock_printer/proto/control_flow.pb.h"
#include "mock_printer/proto/ipp.pb.h"

namespace mock_printer {

std::optional<mocking::TestCase> ParseTestCase(const std::string& textproto) {
  mocking::TestCase parsed;
  if (!google::protobuf::TextFormat::ParseFromString(textproto, &parsed)) {
    return std::nullopt;
  }
  return parsed;
}

mocking::TestCase ParseTestCaseOrDie(const std::string& textproto) {
  std::optional<mocking::TestCase> result = ParseTestCase(textproto);
  CHECK(result.has_value()) << "failed to parse `mocking::TestCase`";
  return std::move(result.value());
}

mocking::HttpProperties ParseHttpPropertiesOrDie(const std::string& textproto) {
  mocking::HttpProperties parsed;
  CHECK(google::protobuf::TextFormat::ParseFromString(textproto, &parsed))
      << "failed to parse `mocking::HttpProperties`";
  return parsed;
}

mocking::IppMessage ParseIppMessageOrDie(const std::string& textproto) {
  mocking::IppMessage parsed;
  CHECK(google::protobuf::TextFormat::ParseFromString(textproto, &parsed))
      << "failed to parse `mocking::IppMessage`";
  return parsed;
}

mocking::Cardinality ParseCardinalityOrDie(const std::string& textproto) {
  mocking::Cardinality parsed;
  CHECK(google::protobuf::TextFormat::ParseFromString(textproto, &parsed))
      << "failed to parse `mocking::Cardinality`";
  return parsed;
}

}  // namespace mock_printer
