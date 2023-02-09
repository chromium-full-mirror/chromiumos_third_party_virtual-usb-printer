// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_TEST_CASE_HOLDER_H_
#define MOCK_PRINTER_TEST_CASE_HOLDER_H_

#include <memory>
#include <string>

#include "argument_variants.h"
#include "mock_printer/proto/control_flow.pb.h"

namespace mock_printer {

class WrappedMatcher;

class TestCaseHolder {
 public:
  static std::unique_ptr<TestCaseHolder> Create(
      const mocking::TestCase& test_case,
      std::unique_ptr<WrappedMatcher> matcher);
  virtual ~TestCaseHolder() = default;

  // Returns whether or not `this` expects any more input messages.
  virtual bool TestCaseFinished() = 0;

  // Returns a description of the test failure. Empty if test has
  // not failed.
  virtual const std::string& TestCaseError() = 0;

  // Attempts to make progress on the contained `mocking::TestCase`.
  // Aborts the program if `TestCaseFinished()` => true.
  //
  // Returns nullptr if the test case cannot proceed (i.e. that
  // `message` does not match any expectation).
  virtual const mocking::Response* Progress(const InputMessage& message) = 0;
};

}  // namespace mock_printer

#endif  // MOCK_PRINTER_TEST_CASE_HOLDER_H_
