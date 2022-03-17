// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_MOCK_PRINTER_H_
#define MOCK_PRINTER_MOCK_PRINTER_H_

#include <memory>
#include <optional>

#include "mock_printer/proto/control_flow.pb.h"
#include "virtual-usb-printer/smart_buffer.h"

namespace mock_printer {

// Top-level implementation of the mock printer.
//
// This class currently provides no direct insight into why a test case
// may have failed; instead, this information is LOG()'d.
class MockPrinter {
 public:
  static std::unique_ptr<MockPrinter> Create(
      const mocking::TestCase& test_case);
  virtual ~MockPrinter() = default;

  // Reads `message` and matches it against the contained test
  // case, returning the appropriate response.
  //
  // Returns nullopt if no match was found.
  //
  // TODO(b/205199712): an optional `SmartBuffer` isn't enough to
  // describe all the possible things we might want the mock printer
  // to do (e.g. to stall indefinitely). We'll need to revisit this
  // method to add functionality.
  virtual std::optional<SmartBuffer> ReceiveDataAndReact(
      const SmartBuffer& message) = 0;
};

}  // namespace mock_printer

#endif  // MOCK_PRINTER_MOCK_PRINTER_H_
