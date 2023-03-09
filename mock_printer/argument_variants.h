// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_ARGUMENT_VARIANTS_H_
#define MOCK_PRINTER_ARGUMENT_VARIANTS_H_

#include <variant>

#include <chromeos/libipp/frame.h>

#include "mock_printer/proto/ipp.pb.h"
#include "mock_printer/proto/control_flow.pb.h"

// This header defines the arguments that the mock printer understands.
// That includes holders for
// *  real messages and
// *  their mock (Protobuf) counterparts.
//
// Currently, mock printer mode only supports IPP messages, but this
// header is written in hopes of making future features a bit easier to
// add.
//
// Most structs defined here do not own their members and must not
// outlive them.

namespace mock_printer {

// Unowned pointers to
// *  a real IPP message to be matched and
// *  a mock IPP message to be matched against.
struct MatchableIppData {
  MatchableIppData(const ipp::Frame& message,
                   const mocking::IppMessage& mock_message);
  ~MatchableIppData();

  const ipp::Frame& ipp_request;
  const mocking::IppMessage& mock_ipp_request;
};

// Pseudo-type-erasing holder for data that we'll pass to
// `WrappedMatcher::Match()`.
using MatchableData = std::variant<MatchableIppData>;

// Counterpart to `MatchableData` exposed by `WrappedTestCaseStep`,
// omitting the mock Protobuf.
using InputMessage = std::variant<const ipp::Frame*>;

// CHECK()s that `message` contains the IPP variant.
MatchableData GetAsIpp(const InputMessage message,
                       const mocking::ExpectationWithResponse& mock_message);

}  // namespace mock_printer

#endif  // MOCK_PRINTER_ARGUMENT_VARIANTS_H_
