// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_WRAPPED_MATCHER_H_
#define MOCK_PRINTER_WRAPPED_MATCHER_H_

#include <memory>

#include "argument_variants.h"
#include "mock_printer/proto/ipp.pb.h"

// This header declares a trivial wrapper class for matching functions
// (e.g. ./ipp_matching.h). This is done to ease unit testing where we
// may wish to exercise direct control over matching behavior without
// relying on the actual implementation.
//
// See also:
// https://google.github.io/googletest/gmock_cook_book.html#mocking-free-functions

namespace mock_printer {

class WrappedMatcher {
 public:
  static std::unique_ptr<WrappedMatcher> Create();
  virtual ~WrappedMatcher() = default;

  // Returns whether the implementation-defined pair of members in
  // `matchable` do match. For example, an IPP-powered `WrappedMatcher`
  // checks `matchable.ipp_request` against
  // `matchable.mock_ipp_request`.
  virtual bool Match(const MatchableData data) = 0;
};

}  // namespace mock_printer

#endif  // MOCK_PRINTER_WRAPPED_MATCHER_H_
