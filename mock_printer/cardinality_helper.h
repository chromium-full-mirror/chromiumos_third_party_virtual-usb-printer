// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_CARDINALITY_HELPER_H_
#define MOCK_PRINTER_CARDINALITY_HELPER_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "mock_printer/proto/control_flow.pb.h"

namespace mock_printer {

// A `mocking::TestCaseStep` can contain multiple
// `mocking::ExpectationWithResponse` messages. As the test case (step)
// progresses, we need to keep track of what `ExpectationWithResponse`s
// have been hit (and those that haven't been hit).
//
// The `CardinalityHelper` interface provides the means to do this.
class CardinalityHelper {
 public:
  // Creates a `CardinalityHelper`
  // *  whose behavior is determined by `cardinality` and
  // *  which is equipped to handle up to `num_expectations`
  //    `ExpectationWithResponse` messages (appropriate to the
  //    `mocking::Cardinality::Type`).
  static std::unique_ptr<CardinalityHelper> Create(
      const mocking::Cardinality& cardinality, std::size_t num_expectations);
  virtual ~CardinalityHelper() = default;

  // Returns the sorted indices of all `ExpectationWithResponse`
  // messages that have not yet been hit.
  virtual std::vector<std::size_t> GetCandidateIndices() = 0;

  // Internally marks the `ExpectationWithResponse` at `index` as hit.
  virtual void MarkIndexVisited(std::size_t index) = 0;

  virtual bool Fulfilled() = 0;
  virtual bool Saturated() = 0;
};

}  // namespace mock_printer

#endif  // MOCK_PRINTER_CARDINALITY_HELPER_H_
