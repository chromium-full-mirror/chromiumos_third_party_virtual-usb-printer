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

namespace mock_printer {

base::Optional<mocking::IppMessage> ParseTextproto(
    const std::string& textproto) {
  mocking::IppMessage result;
  if (!google::protobuf::TextFormat::ParseFromString(textproto, &result)) {
    return base::nullopt;
  }
  return result;
}

mocking::IppMessage ParseTextprotoOrDie(const std::string& textproto) {
  base::Optional<mocking::IppMessage> result = ParseTextproto(textproto);
  CHECK(result.has_value());
  return std::move(result.value());
}

}  // namespace mock_printer
