// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wrapped_test_case_step.h"

#include <memory>
#include <utility>
#include <vector>

#include <base/check.h>
#include <chromeos/libipp/attribute.h>
#include <chromeos/libipp/frame.h>
#include <gmock/gmock.h>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "parse_textproto.h"
#include "mock_printer/proto/control_flow.pb.h"
#include "mock_printer/proto/ipp.pb.h"
#include "wrapped_matcher.h"

// This file provides unit tests for `WrappedTestCaseStep`. Testing
// is performed by
// *  using custom (invalid, illegal-in-production) messages that hide
//    individual numeric markers inside the `ipp::Request`s and
//    `mocking::IppMessage`s and
// *  injecting a `WrappedMatcher` that knows how to find and correlate
//    the markers.
//
// The convention adopted in this unit test is to hide the tell-tale
// numeric markers
// *  in the `job_id` of the `ipp::Request`,
// *  in the `version_number` of the `mocking::IppMessage`, and
// *  in the `pause_seconds` of the `mocking::Response`.

namespace mock_printer {
namespace {

using ::testing::IsNull;
using ::testing::NotNull;

// Convenience function that builds test data.
std::unique_ptr<ipp::Frame> MakeRequest(int value) {
  auto request =
      std::make_unique<ipp::Frame>(ipp::Operation::Get_Job_Attributes);
  auto coll = request->Groups(ipp::GroupTag::operation_attributes).begin();
  CHECK(coll->AddAttr("job-id", value) == ipp::Code::kOK);
  return request;
}

// Defines a custom `WrappedMatcher` that doesn't rely on the actual
// semantics of IPP matching.
class FakeWrappedMatcher : public WrappedMatcher {
 public:
  static std::unique_ptr<WrappedMatcher> Create() {
    return std::make_unique<FakeWrappedMatcher>();
  }
  FakeWrappedMatcher() = default;
  ~FakeWrappedMatcher() override = default;

  // See the file-level comment about the tell-tale numeric markers.
  bool Match(const MatchableData data) override {
    auto* ipp_data = std::get_if<MatchableIppData>(&data);
    CHECK(ipp_data) << "BUG: expected `MatchableIppData` variant";

    std::vector<int32_t> job_id;

    const ipp::Collection& coll =
        ipp_data->ipp_request.Groups(ipp::GroupTag::operation_attributes)[0];
    auto attr = coll.GetAttr("job-id");
    CHECK(attr != coll.end()) << "BUG: failed to get job_id";

    CHECK(attr->GetValues(job_id) == ipp::Code::kOK)
        << "BUG: invalid type of job_id";
    return job_id[0] == ipp_data->mock_ipp_request.version_number();
  }
};

// Tests that a trivial test case step with exactly one expectation
// succeeds.
TEST(WrappedTestCaseStep, SingleExpectationSuccess) {
  const auto test_case = ParseTestCaseOrDie(R"pb(
    steps {
      cardinality { type: ALL_IN_ORDER }
      expectation_with_response {
        expectation { ipp_matcher { version_number: 1138 } }
        response { pause_seconds: 1139 }
      }
    }
  )pb");
  auto matcher = FakeWrappedMatcher::Create();
  auto step = WrappedTestCaseStep::Create(test_case.steps(0), matcher.get());

  auto request = MakeRequest(1138);
  InputMessage input(request.get());
  ProgressionResult result = step->Progress(input);

  EXPECT_TRUE(result.match_found);
  EXPECT_EQ(result.step_status, StepStatus::kSaturated);
  ASSERT_THAT(result.response, NotNull());
  EXPECT_EQ(result.response->pause_seconds(), 1139);
}

// Tests the case where an `InputMessage` matches nothing in the
// particular `WrappedTestCaseStep`.
TEST(WrappedTestCaseStep, NoMatchingExpectation) {
  const auto test_case = ParseTestCaseOrDie(R"pb(
    steps {
      cardinality { type: ALL_IN_ORDER }
      expectation_with_response {
        expectation { ipp_matcher { version_number: 1138 } }
        response { pause_seconds: 1139 }
      }
    }
  )pb");
  auto matcher = FakeWrappedMatcher::Create();
  auto step = WrappedTestCaseStep::Create(test_case.steps(0), matcher.get());

  // 0 != 1138; the matcher will not correlate this with the given
  // expectation.
  auto request = MakeRequest(0);
  InputMessage input(request.get());
  ProgressionResult result = step->Progress(input);

  EXPECT_FALSE(result.match_found);
  EXPECT_EQ(result.step_status, StepStatus::kUnfulfilled);
  EXPECT_THAT(result.response, IsNull());
}

// Tests that a test case step with multiple expectations succeeds.
TEST(WrappedTestCaseStep, MultiExpectationSuccess) {
  const auto test_case = ParseTestCaseOrDie(R"pb(
    steps {
      cardinality { type: ALL_IN_ANY_ORDER }
      expectation_with_response {
        expectation { ipp_matcher { version_number: 1138 } }
        response { pause_seconds: 1139 }
      }
      expectation_with_response {
        expectation { ipp_matcher { version_number: 2038 } }
        response { pause_seconds: 2039 }
      }
      expectation_with_response {
        expectation { ipp_matcher { version_number: 3038 } }
        response { pause_seconds: 3039 }
      }
    }
  )pb");
  auto matcher = FakeWrappedMatcher::Create();
  auto step = WrappedTestCaseStep::Create(test_case.steps(0), matcher.get());

  // Progress the step by hitting the third ExpectationWithResponse.
  auto first_request = MakeRequest(3038);
  InputMessage first_input(first_request.get());
  ProgressionResult first_result = step->Progress(first_input);

  EXPECT_TRUE(first_result.match_found);
  EXPECT_EQ(first_result.step_status, StepStatus::kUnfulfilled);
  ASSERT_THAT(first_result.response, NotNull());
  EXPECT_EQ(first_result.response->pause_seconds(), 3039);

  // Progress the step by hitting the second ExpectationWithResponse.
  auto second_request = MakeRequest(2038);
  InputMessage second_input(second_request.get());
  ProgressionResult second_result = step->Progress(second_input);

  EXPECT_TRUE(second_result.match_found);
  EXPECT_EQ(second_result.step_status, StepStatus::kUnfulfilled);
  ASSERT_THAT(second_result.response, NotNull());
  EXPECT_EQ(second_result.response->pause_seconds(), 2039);

  // Progress and saturate the step by hitting the first
  // ExpectationWithResponse.
  auto third_request = MakeRequest(1138);
  InputMessage third_input(third_request.get());
  ProgressionResult third_result = step->Progress(third_input);

  EXPECT_TRUE(third_result.match_found);
  EXPECT_EQ(third_result.step_status, StepStatus::kSaturated);
  ASSERT_THAT(third_result.response, NotNull());
  EXPECT_EQ(third_result.response->pause_seconds(), 1139);
}

// Tests that a test case step can fulfill without saturating.
TEST(WrappedTestCaseStep, FulfillWithoutSaturating) {
  const auto test_case = ParseTestCaseOrDie(R"pb(
    steps {
      cardinality {
        type: SOME_OF
        count: { at_least: 2 }
      }
      expectation_with_response {
        expectation { ipp_matcher { version_number: 1138 } }
        response { pause_seconds: 1139 }
      }
      expectation_with_response {
        expectation { ipp_matcher { version_number: 2038 } }
        response { pause_seconds: 2039 }
      }
      expectation_with_response {
        expectation { ipp_matcher { version_number: 3038 } }
        response { pause_seconds: 3039 }
      }
    }
  )pb");
  auto matcher = FakeWrappedMatcher::Create();
  auto step = WrappedTestCaseStep::Create(test_case.steps(0), matcher.get());

  // Progress the step by hitting the third ExpectationWithResponse.
  auto first_request = MakeRequest(3038);
  InputMessage first_input(first_request.get());
  ProgressionResult first_result = step->Progress(first_input);

  EXPECT_TRUE(first_result.match_found);
  EXPECT_EQ(first_result.step_status, StepStatus::kUnfulfilled);
  ASSERT_THAT(first_result.response, NotNull());
  EXPECT_EQ(first_result.response->pause_seconds(), 3039);

  // Progress the step by hitting the second ExpectationWithResponse.
  // This will fulfill the step without saturating it.
  auto second_request = MakeRequest(2038);
  InputMessage second_input(second_request.get());
  ProgressionResult second_result = step->Progress(second_input);

  EXPECT_TRUE(second_result.match_found);
  EXPECT_EQ(second_result.step_status, StepStatus::kFulfilled);
  ASSERT_THAT(second_result.response, NotNull());
  EXPECT_EQ(second_result.response->pause_seconds(), 2039);

  // Make no progress in the step by comparing against a non-matching
  // input message.
  auto third_request = MakeRequest(0);
  InputMessage third_input(third_request.get());
  ProgressionResult third_result = step->Progress(third_input);

  EXPECT_FALSE(third_result.match_found);
  EXPECT_EQ(third_result.step_status, StepStatus::kFulfilled);
  ASSERT_THAT(third_result.response, IsNull());
}

}  // namespace
}  // namespace mock_printer
