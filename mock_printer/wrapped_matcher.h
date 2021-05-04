// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_WRAPPED_MATCHER_H_
#define MOCK_PRINTER_WRAPPED_MATCHER_H_

#include <memory>
#include <variant>

#include <chromeos/libipp/ipp.h>

#include "proto/ipp.pb.h"

// This header declares a trivial wrapper class for matching functions
// (e.g. ./ipp_matching.h). This is done to ease unit testing where we
// may wish to exercise direct control over matching behavior without
// relying on the actual implementation.
//
// See also:
// https://google.github.io/googletest/gmock_cook_book.html#mocking-free-functions

namespace mock_printer {

// Unowned pointers to
// *  a real IPP message to be matched and
// *  a mock IPP message to be matched against.
//
// This struct does not own its members and must not outlive them.
struct MatchableIppData {
  MatchableIppData(const ipp::Request* message,
                   const mocking::IppMessage& mock_message);
  ~MatchableIppData();

  const ipp::Request* ipp_request;
  const mocking::IppMessage& mock_ipp_request;
};

// Pseudo-type-erasing holder for data that we'll pass to
// `WrappedMatcher::Match()`.
using MatchableData = std::variant<MatchableIppData>;

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
