// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cardinality_helper.h"

#include <gmock/gmock.h>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "parse_textproto.h"
#include "mock_printer/proto/control_flow.pb.h"

namespace mock_printer {
namespace {

using ::testing::ElementsAre;

// This test suite provides rudimentary unit tests for the
// `CardinalityHelper` class.
//
// A proper caller would check `CardinalityHelper::Saturated()` at each
// turn, but we don't bother in these unit tests.

TEST(CardinalityHelperTest, AllInOrder) {
  const auto cardinality = ParseCardinalityOrDie(R"pb(
    type: ALL_IN_ORDER
  )pb");

  // Creates a `CardinalityHelper` that will process a series of five
  // `ExpectationWithResponse` messages in order.
  const auto helper = CardinalityHelper::Create(cardinality, 5ull);
  ASSERT_FALSE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(0ull));
  helper->MarkIndexVisited(0ull);
  ASSERT_FALSE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(1ull));
  helper->MarkIndexVisited(1ull);
  ASSERT_FALSE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(2ull));
  helper->MarkIndexVisited(2ull);
  ASSERT_FALSE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(3ull));
  helper->MarkIndexVisited(3ull);
  ASSERT_FALSE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(4ull));
  helper->MarkIndexVisited(4ull);

  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_TRUE(helper->Saturated());
}

TEST(CardinalityHelperTest, AllInAnyOrder) {
  const auto cardinality = ParseCardinalityOrDie(R"pb(
    type: ALL_IN_ANY_ORDER
  )pb");

  // Creates a `CardinalityHelper` that will process a series of five
  // `ExpectationWithResponse` messages in any order.
  const auto helper = CardinalityHelper::Create(cardinality, 5ull);

  ASSERT_THAT(helper->GetCandidateIndices(),
              ElementsAre(0ull, 1ull, 2ull, 3ull, 4ull));

  // Marks indices visited in a random order.
  helper->MarkIndexVisited(4ull);
  ASSERT_THAT(helper->GetCandidateIndices(),
              ElementsAre(0ull, 1ull, 2ull, 3ull));
  helper->MarkIndexVisited(0ull);
  ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(1ull, 2ull, 3ull));
  helper->MarkIndexVisited(2ull);
  ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(1ull, 3ull));
  helper->MarkIndexVisited(3ull);
  ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(1ull));
  helper->MarkIndexVisited(1ull);

  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_TRUE(helper->Saturated());
}

TEST(CardinalityHelperTest, SomeOfWithAtLeast) {
  const auto cardinality = ParseCardinalityOrDie(R"pb(
    type: SOME_OF
    count { at_least: 2 }
  )pb");

  // Creates a `CardinalityHelper` that will process a selection of at
  // least two (of five possible) `ExpectationWithResponse` messages in
  // any order.
  const auto helper = CardinalityHelper::Create(cardinality, 5ull);

  ASSERT_THAT(helper->GetCandidateIndices(),
              ElementsAre(0ull, 1ull, 2ull, 3ull, 4ull));

  // Gets this `CardinalityHelper` fulfilled by hitting two indices.
  helper->MarkIndexVisited(3ull);
  ASSERT_FALSE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  helper->MarkIndexVisited(2ull);
  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(0ull, 1ull, 4ull));

  // Saturates this `CardinalityHelper` by hitting the rest.
  helper->MarkIndexVisited(1ull);
  helper->MarkIndexVisited(0ull);
  helper->MarkIndexVisited(4ull);
  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_TRUE(helper->Saturated());
}

TEST(CardinalityHelperTest, SomeOfWithAtMost) {
  const auto cardinality = ParseCardinalityOrDie(R"pb(
    type: SOME_OF
    count { at_most: 2 }
  )pb");

  // Creates a `CardinalityHelper` that will process a selection of at
  // most two (of five possible) `ExpectationWithResponse` messages in
  // any order.
  const auto helper = CardinalityHelper::Create(cardinality, 5ull);

  ASSERT_THAT(helper->GetCandidateIndices(),
              ElementsAre(0ull, 1ull, 2ull, 3ull, 4ull));

  // No `at_least` is given, so this `CardinalityHelper` could be
  // fulfilled from the start.
  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  // Saturates the `CardinalityHelper` by hitting two elements.
  helper->MarkIndexVisited(3ull);
  helper->MarkIndexVisited(1ull);

  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_TRUE(helper->Saturated());
}

TEST(CardinalityHelperTest, SomeOfWithAtLeastAndAtMost) {
  const auto cardinality = ParseCardinalityOrDie(R"pb(
    type: SOME_OF
    count { at_least: 2 at_most: 3 }
  )pb");

  // Creates a `CardinalityHelper` that will process a selection of
  // *  at least two and
  // *  at most three
  // of five possible `ExpectationWithResponse` messages in any order.
  const auto helper = CardinalityHelper::Create(cardinality, 5ull);

  ASSERT_THAT(helper->GetCandidateIndices(),
              ElementsAre(0ull, 1ull, 2ull, 3ull, 4ull));
  ASSERT_FALSE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  // Fulfills the `CardinalityHelper` by hitting two elements.
  helper->MarkIndexVisited(1ull);
  helper->MarkIndexVisited(2ull);
  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());
  ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(0ull, 3ull, 4ull));

  // Saturates it by hitting one more (totaling three).
  helper->MarkIndexVisited(4ull);
  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_TRUE(helper->Saturated());
}

TEST(CardinalityHelperTest, SomeOfWithExactly) {
  const auto cardinality = ParseCardinalityOrDie(R"pb(
    type: SOME_OF
    count { exactly: 3 }
  )pb");

  // Creates a `CardinalityHelper` that will process a selection of
  // exactly three of five possible `ExpectationWithResponse` messages
  // in any order.
  const auto helper = CardinalityHelper::Create(cardinality, 5ull);

  ASSERT_THAT(helper->GetCandidateIndices(),
              ElementsAre(0ull, 1ull, 2ull, 3ull, 4ull));
  ASSERT_FALSE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  // Gets close to fulfilling (and saturating) the `CardinalityHelper`
  // by hitting two elements.
  helper->MarkIndexVisited(3ull);
  helper->MarkIndexVisited(2ull);
  ASSERT_FALSE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());
  ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(0ull, 1ull, 4ull));

  // Fulfills and saturates the `CardinalityHelper` by hitting one more
  // element (totaling three).
  helper->MarkIndexVisited(0ull);
  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_TRUE(helper->Saturated());
}

TEST(CardinalityHelperTest, RepeatedWithAtLeast) {
  const auto cardinality = ParseCardinalityOrDie(R"pb(
    type: REPEATED
    count { at_least: 3 }
  )pb");

  // Creates a `CardinalityHelper` that will process an
  // `ExpectationWithResponse` message at least three times.
  const auto helper = CardinalityHelper::Create(cardinality, 1ull);

  ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(0ull));
  ASSERT_FALSE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  // Fulfills the `CardinalityHelper`.
  helper->MarkIndexVisited(0ull);
  helper->MarkIndexVisited(0ull);
  helper->MarkIndexVisited(0ull);
  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  // The `CardinalityHelper` does not saturate.
  for (int i = 0; i < 20; i++) {
    ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(0ull));
    helper->MarkIndexVisited(0ull);
  }
  ASSERT_FALSE(helper->Saturated());
}

TEST(CardinalityHelperTest, RepeatedWithAtMost) {
  const auto cardinality = ParseCardinalityOrDie(R"pb(
    type: REPEATED
    count { at_most: 3 }
  )pb");

  // Creates a `CardinalityHelper` that will process an
  // `ExpectationWithResponse` message at most three times.
  const auto helper = CardinalityHelper::Create(cardinality, 1ull);

  ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(0ull));
  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  // In this configuration, the `CardinalityHelper` starts out
  // fulfilled (but not saturated).
  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  // Saturates the `CardinalityHelper` by hitting the same index
  // thrice over.
  helper->MarkIndexVisited(0ull);
  helper->MarkIndexVisited(0ull);
  helper->MarkIndexVisited(0ull);

  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_TRUE(helper->Saturated());
}

TEST(CardinalityHelperTest, RepeatedWithAtLeastAndAtMost) {
  const auto cardinality = ParseCardinalityOrDie(R"pb(
    type: REPEATED
    count { at_least: 2 at_most: 3 }
  )pb");

  // Creates a `CardinalityHelper` that will process an
  // `ExpectationWithResponse` message
  // *  at least two times and
  // *  at most three times.
  const auto helper = CardinalityHelper::Create(cardinality, 1ull);

  ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(0ull));
  ASSERT_FALSE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  // Fulfills the `CardinalityHelper` by hitting the same index twice.
  helper->MarkIndexVisited(0ull);
  helper->MarkIndexVisited(0ull);
  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  // Saturates it by hitting it once more.
  helper->MarkIndexVisited(0ull);
  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_TRUE(helper->Saturated());
}

TEST(CardinalityHelperTest, RepeatedWithExactly) {
  const auto cardinality = ParseCardinalityOrDie(R"pb(
    type: REPEATED
    count { exactly: 3 }
  )pb");

  // Creates a `CardinalityHelper` that will process an
  // `ExpectationWithResponse` message exactly three times.
  const auto helper = CardinalityHelper::Create(cardinality, 1ull);

  ASSERT_THAT(helper->GetCandidateIndices(), ElementsAre(0ull));
  ASSERT_FALSE(helper->Fulfilled());
  ASSERT_FALSE(helper->Saturated());

  // Fulfills and saturates the `CardinalityHelper` by hitting it three
  // times.
  helper->MarkIndexVisited(0ull);
  helper->MarkIndexVisited(0ull);
  helper->MarkIndexVisited(0ull);
  ASSERT_TRUE(helper->Fulfilled());
  ASSERT_TRUE(helper->Saturated());
}

}  // namespace
}  // namespace mock_printer
