// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wrapped_test_case_step.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <base/check.h>

#include "argument_variants.h"
#include "cardinality_helper.h"
#include "mock_printer/proto/control_flow.pb.h"
#include "wrapped_matcher.h"

namespace mock_printer {
namespace {

class WrappedIppTestCaseStep : public WrappedTestCaseStep {
 public:
  WrappedIppTestCaseStep(const mocking::TestCaseStep& step,
                         WrappedMatcher* matcher)
      : step_(step), matcher_(matcher) {
    cardinality_helper_ = CardinalityHelper::Create(
        step.cardinality(), step.expectation_with_response_size());
  }

  ~WrappedIppTestCaseStep() override = default;

  ProgressionResult Progress(const InputMessage message) override {
    CHECK(!cardinality_helper_->Saturated())
        << "BUG: called `Progress()` on saturated step";

    const std::vector<std::size_t> candidate_indices =
        cardinality_helper_->GetCandidateIndices();
    CHECK(!candidate_indices.empty())
        << "BUG: no candidate indices after cardinality_helper_ claimed not to "
           "be Saturated()";

    for (const std::size_t index : candidate_indices) {
      const auto& proto =
          step_.expectation_with_response(static_cast<int>(index));
      const MatchableData matchable_data = GetAsIpp(message, proto);
      if (!matcher_->Match(matchable_data)) {
        continue;
      }

      cardinality_helper_->MarkIndexVisited(index);
      StepStatus status =
          cardinality_helper_->Saturated()   ? StepStatus::kSaturated
          : cardinality_helper_->Fulfilled() ? StepStatus::kFulfilled
                                             : StepStatus::kUnfulfilled;
      if (proto.has_response()) {
        return {/*match_found=*/true, status, &proto.response()};
      }
      return {/*match_found=*/true, status, nullptr};
    }

    // Having fallen out of the for loop, no matches were found - i.e.
    // no progress was made, the step cannot possibly be saturated, and
    // there's no response the caller can get from here.
    StepStatus status = cardinality_helper_->Fulfilled()
                            ? StepStatus::kFulfilled
                            : StepStatus::kUnfulfilled;
    return {/*match_found=*/false, status, /*response=*/nullptr};
  }

 private:
  const mocking::TestCaseStep step_;
  WrappedMatcher* matcher_;
  std::unique_ptr<CardinalityHelper> cardinality_helper_;
};

}  // namespace

// static
std::unique_ptr<WrappedTestCaseStep> WrappedTestCaseStep::Create(
    const mocking::TestCaseStep& step, WrappedMatcher* matcher) {
  return std::make_unique<WrappedIppTestCaseStep>(step, matcher);
}

}  // namespace mock_printer
