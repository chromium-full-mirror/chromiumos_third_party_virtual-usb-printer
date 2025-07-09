// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_case_holder.h"

#include <cstddef>
#include <utility>
#include <vector>

#include <base/check_op.h>
#include <base/format_macros.h>
#include <base/notreached.h>
#include <base/strings/stringprintf.h>

#include "wrapped_matcher.h"
#include "wrapped_test_case_step.h"

namespace mock_printer {
namespace {

class IppTestCaseHolder : public TestCaseHolder {
 public:
  IppTestCaseHolder(const mocking::TestCase& test_case,
                    std::unique_ptr<WrappedMatcher> matcher)
      : test_case_(test_case), matcher_(std::move(matcher)), step_index_(0) {
    for (const mocking::TestCaseStep& step : test_case.steps()) {
      wrapped_test_case_steps_.push_back(
          WrappedTestCaseStep::Create(step, matcher_.get()));
    }
  }

  ~IppTestCaseHolder() override = default;

  const mocking::Response* Progress(const InputMessage& message) override {
    CHECK(!TestCaseFinished())
        << "BUG: called `Progress()` on finished test case";
    while (!TestCaseFinished()) {
      // If a match is found, the `WrappedTestCase` may change the
      // step state from `kUnfulfilled` to `kFulfilled` or to
      // `kSaturated`.
      const ProgressionResult result =
          wrapped_test_case_steps_.at(step_index_)->Progress(message);

      // The state of the whole test case is determined by
      // *  whether a match was found and
      // *  the state of the test case step.
      if (result.match_found) {
        if (result.step_status == StepStatus::kSaturated) {
          // This match saturates the step. We move the test case
          // forward to prohibit future calls to `Progress()` from
          // revisiting this saturated step.
          ++step_index_;
        }
        return result.response;
      }

      // Handle the case where a match wasn't found.
      switch (result.step_status) {
        case StepStatus::kUnfulfilled:
          error_ = base::StringPrintf("Test failed at `step_index_` %" PRIuS
                                      ": no matching expectation",
                                      step_index_);
          return nullptr;
        case StepStatus::kFulfilled:
          // The step was already fulfilled, but the current `message`
          // matches no expectation in this step. We need to search
          // forward for a match.
          ++step_index_;
          continue;
        case StepStatus::kSaturated:
          // This state should be impossible to reach: the only way
          // for a step to reach saturation is if a match is found,
          // and when a match is found, the test case should advance
          // to prevent revisiting a saturated step (see above).
          NOTREACHED() << "BUG: step saturated but no match found";
      }
      NOTREACHED() << "BUG: no-match case flowed outside switch statement";
    }

    error_ = "Test case exhausted: no matching expectation";
    return nullptr;
  }

  bool TestCaseFinished() override {
    return step_index_ >= test_case_.steps_size();
  }

  const std::string& TestCaseError() override { return error_; }

 private:
  mocking::TestCase test_case_;
  std::unique_ptr<WrappedMatcher> matcher_;
  std::vector<std::unique_ptr<mock_printer::WrappedTestCaseStep>>
      wrapped_test_case_steps_;

  // The index of the current step in the `test_case_`.
  std::size_t step_index_;

  // An error message describing how the test failed (if it did).
  std::string error_;
};

}  // namespace

// static
std::unique_ptr<TestCaseHolder> TestCaseHolder::Create(
    const mocking::TestCase& test_case,
    std::unique_ptr<WrappedMatcher> matcher) {
  return std::make_unique<IppTestCaseHolder>(test_case, std::move(matcher));
}

}  // namespace mock_printer
