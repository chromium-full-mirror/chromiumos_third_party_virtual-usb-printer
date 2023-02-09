// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wrapped_matcher.h"

#include <utility>

#include <base/check.h>

#include "ipp_matching.h"

namespace mock_printer {

namespace {

class WrappedIppMatcher : public WrappedMatcher {
 public:
  WrappedIppMatcher() = default;
  ~WrappedIppMatcher() override = default;

  bool Match(const MatchableData data) override {
    auto* ipp_data = std::get_if<MatchableIppData>(&data);
    CHECK(ipp_data) << "BUG: expected `MatchableIppData` variant";
    return IppMessageMatches(ipp_data->ipp_request, ipp_data->mock_ipp_request);
  }
};

}  // namespace

// static
std::unique_ptr<WrappedMatcher> WrappedMatcher::Create() {
  return std::make_unique<WrappedIppMatcher>();
}

}  // namespace mock_printer
