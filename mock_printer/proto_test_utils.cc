// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "proto_test_utils.h"

#include <string>

#include <google/protobuf/text_format.h>

#include <base/check.h>
#include <base/logging.h>

namespace mock_printer {

mocking::IppMessage ParseTestProto(const std::string& textproto) {
  mocking::IppMessage result;
  // Parsing textprotos correctly is a prerequisite; it's not in scope
  // for Mock Printer Mode to deal with.
  CHECK(google::protobuf::TextFormat::ParseFromString(textproto, &result))
      << "BUG: invalid textproto ``" << textproto << "''";
  return result;
}

}  // namespace mock_printer
