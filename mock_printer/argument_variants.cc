// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "argument_variants.h"

#include <utility>

#include <base/check.h>
#include <chromeos/libipp/ipp.h>

namespace mock_printer {

MatchableIppData::MatchableIppData(const ipp::Request* message,
                                   const mocking::IppMessage& mock_message)
    : ipp_request(message), mock_ipp_request(mock_message) {}
MatchableIppData::~MatchableIppData() = default;

MatchableData GetAsIpp(const InputMessage message,
                       const mocking::ExpectationWithResponse& mock_message) {
  auto* ipp_message = std::get_if<const ipp::Request*>(&message);
  CHECK(ipp_message) << "BUG: expected ipp::Request* variant here";
  return MatchableIppData(*ipp_message,
                          mock_message.expectation().ipp_matcher());
}

}  // namespace mock_printer
