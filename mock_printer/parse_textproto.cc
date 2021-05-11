// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "parse_textproto.h"

#include <string>
#include <utility>

#include <google/protobuf/text_format.h>

#include <base/check.h>
#include <base/logging.h>
#include <base/optional.h>

#include "proto/control_flow.pb.h"
#include "proto/ipp.pb.h"

namespace mock_printer {

base::Optional<mocking::TestCase> ParseTestCase(const std::string& textproto) {
  mocking::TestCase parsed;
  if (!google::protobuf::TextFormat::ParseFromString(textproto, &parsed)) {
    return base::nullopt;
  }
  return parsed;
}

mocking::TestCase ParseTestCaseOrDie(const std::string& textproto) {
  base::Optional<mocking::TestCase> result = ParseTestCase(textproto);
  CHECK(result.has_value()) << "failed to parse `mocking::TestCase`";
  return std::move(result.value());
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
