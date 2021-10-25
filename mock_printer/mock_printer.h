// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_MOCK_PRINTER_H_
#define MOCK_PRINTER_MOCK_PRINTER_H_

#include <memory>

#include "virtual-usb-printer/smart_buffer.h"

namespace mock_printer {

// Top-level implementation of the mock printer.
class MockPrinter {
 public:
  static std::unique_ptr<MockPrinter> Create();
  virtual ~MockPrinter() = default;

  // Uses the `SmartBuffer` to do nothing for the moment.
  virtual void DoNothing(const SmartBuffer& message) = 0;
};

}  // namespace mock_printer

#endif  // MOCK_PRINTER_MOCK_PRINTER_H_
