// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cardinality_helper.h"

#include <algorithm>
#include <cstdint>
#include <numeric>

#include <base/check_op.h>
#include <base/notreached.h>
#include <base/numerics/checked_math.h>

namespace mock_printer {
namespace {

// Helper function that returns a `Count` with `Count::exactly` set to
// `num_expectations`. Used to convert `Type::ALL_IN_ANY_ORDER` to
// `Type::SOME_OF`.
mocking::Cardinality::Count SomeOfWithExactly(std::size_t num_expectations) {
  mocking::Cardinality::Count count;
  count.set_exactly(base::checked_cast<int32_t>(num_expectations));
  return count;
}

class AllInOrder : public CardinalityHelper {
 public:
  explicit AllInOrder(const std::size_t num_expectations)
      : limit_(num_expectations), watermark_(0) {}
  ~AllInOrder() override = default;

  // The vector return type is necessary to comply with the interface,
  // but `AllInOrder` always returns a vector with exactly one value in
  // it, and that will be the first un-hit index. There is no doubling
  // up and no skipping.
  std::vector<std::size_t> GetCandidateIndices() override {
    CHECK(!Saturated());
    return {
        watermark_,
    };
  }

  // `index` is necessary to comply with the interface, but as with the
  // return value of `GetCandidateIndices()`, the value is unambiguous
  // and not useful to our internal bookkeeping.
  void MarkIndexVisited(const std::size_t index) override {
    CHECK(!Saturated());
    CHECK_EQ(index, watermark_);
    watermark_++;
  }

  bool Fulfilled() override { return watermark_ == limit_; }

  bool Saturated() override { return Fulfilled(); }

 private:
  const std::size_t limit_;
  std::size_t watermark_;
};

class SomeOf : public CardinalityHelper {
 public:
  // `count` is passed straight through from the related
  // `mocking::TestCaseStep` and describes how many times the step
  // can be hit. This is
  SomeOf(const mocking::Cardinality::Count& count,
         const std::size_t num_expectations)
      : count_(count),
        limit_(num_expectations),
        unvisited_indices_(num_expectations) {
    CHECK_LE(count.at_least(), num_expectations);
    CHECK_LE(count.at_most(), num_expectations);
    if (count.at_most()) {
      CHECK_LE(count.at_least(), count.at_most());
    }
    CHECK_LE(count.exactly(), num_expectations);
    std::iota(unvisited_indices_.begin(), unvisited_indices_.end(), 0);
  }
  ~SomeOf() override = default;

  std::vector<std::size_t> GetCandidateIndices() override {
    CHECK(!Saturated());
    return unvisited_indices_;
  }

  void MarkIndexVisited(const std::size_t index) override {
    CHECK(!Saturated());
    auto iter =
        std::find(unvisited_indices_.begin(), unvisited_indices_.end(), index);
    // Iterator types do not expose `operator<<`, precluding usage of
    // `CHECK_NE()` here.
    CHECK(iter != unvisited_indices_.end());
    CHECK_LT(index, limit_);
    unvisited_indices_.erase(iter);
  }

  bool Fulfilled() override {
    if (count_.exactly()) {
      return limit_ - unvisited_indices_.size() == count_.exactly();
    } else if (count_.at_least()) {
      return limit_ - unvisited_indices_.size() >= count_.at_least();
    }
    // `count_.at_most()` has no bearing on fulfillment. If `at_most` is
    // set by itself, the current test case step could be fulfilled from
    // time zero.
    return true;
  }

  bool Saturated() override {
    if (count_.exactly()) {
      return Fulfilled();
    } else if (count_.at_most()) {
      return limit_ - unvisited_indices_.size() == count_.at_most();
    }
    // If `at_least` is set by itself, the current test case step
    // saturates when all steps are hit.
    return unvisited_indices_.empty();
  }

 private:
  const mocking::Cardinality::Count count_;
  const std::size_t limit_;
  std::vector<std::size_t> unvisited_indices_;
};

class Repeated : public CardinalityHelper {
 public:
  explicit Repeated(const mocking::Cardinality::Count& count)
      : count_(count), repetition_count_(0) {}
  ~Repeated() override = default;

  std::vector<std::size_t> GetCandidateIndices() override {
    CHECK(!Saturated());
    return {
        0ull,
    };
  }

  void MarkIndexVisited(const std::size_t index) override {
    CHECK(!Saturated());
    repetition_count_++;
  }

  bool Fulfilled() override {
    if (count_.exactly()) {
      return count_.exactly() == repetition_count_;
    } else if (count_.at_least()) {
      return repetition_count_ >= count_.at_least();
    }
    // `count_.at_most()` has no bearing on fulfillment. If `at_most` is
    // set by itself, the current test case step could be fulfilled from
    // time zero.
    return true;
  }

  bool Saturated() override {
    if (count_.exactly()) {
      return Fulfilled();
    } else if (count_.at_most()) {
      return repetition_count_ == count_.at_most();
    }
    // `count_.at_least()` has no bearing on saturation. If `at_least`
    // is set by itself, the current test case step never saturates.
    return false;
  }

 private:
  const mocking::Cardinality::Count count_;
  std::size_t repetition_count_;
};

}  // namespace

// static
std::unique_ptr<CardinalityHelper> CardinalityHelper::Create(
    const mocking::Cardinality& cardinality,
    const std::size_t num_expectations) {
  switch (cardinality.type()) {
    case mocking::Cardinality::ALL_IN_ORDER:
      return std::make_unique<AllInOrder>(num_expectations);
    case mocking::Cardinality::ALL_IN_ANY_ORDER:
      return std::make_unique<SomeOf>(SomeOfWithExactly(num_expectations),
                                      num_expectations);
    case mocking::Cardinality::SOME_OF:
      return std::make_unique<SomeOf>(cardinality.count(), num_expectations);
    case mocking::Cardinality::REPEATED:
      return std::make_unique<Repeated>(cardinality.count());
    default:
      break;
  }
  // The generated C++ code doesn't quite allow us to exhaust every
  // possible variant of `Cardinality::Type`.
  NOTREACHED();
}

}  // namespace mock_printer
