// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_WRAPPED_TEST_CASE_STEP_H_
#define MOCK_PRINTER_WRAPPED_TEST_CASE_STEP_H_

#include <memory>

#include "mock_printer/proto/control_flow.pb.h"
#include "wrapped_matcher.h"

// This header declares the `WrappedTestCaseStep` class.
// `WrappedTestCaseStep` powers the core control flow of the mock
// printer mode.
//
// Refer to `README.md` for an overview of where this class fits.

namespace mock_printer {

// Represents the state of a `mocking::TestCaseStep`.
enum class StepStatus {
  kUnfulfilled,
  kFulfilled,

  // Note that `kSaturated` logically encloses `kFulfilled`.
  kSaturated,
};

struct ProgressionResult {
  // Answers the question "did the call to
  // `WrappedTestCaseStep::Progress()` match any expectation?
  bool match_found;

  // Irrespective of the above, describes the current state of the
  // contained `mocking::TestCaseStep`. Being independent of
  // `match_found` is important because this state may determine
  // whether the test case should consider the next step (e.g. if we are
  // `kFulfilled`) or if we should fail immediately (e.g.
  // `kUnfulfilled`).
  StepStatus step_status;

  // This member is nullptr if `!match_found`.
  const mocking::Response* response;
};

class WrappedTestCaseStep {
 public:
  // Caller must ensure that this does not outlive `matcher`.
  static std::unique_ptr<WrappedTestCaseStep> Create(
      const mocking::TestCaseStep& step, WrappedMatcher* matcher);
  virtual ~WrappedTestCaseStep() = default;

  // Attempts to make progress on the contained `mocking::TestCaseStep`.
  // CHECK()s that the contained `mocking::TestCaseStep` is not
  // saturated.
  virtual ProgressionResult Progress(const InputMessage message) = 0;
};

}  // namespace mock_printer

#endif  // MOCK_PRINTER_WRAPPED_TEST_CASE_STEP_H_
