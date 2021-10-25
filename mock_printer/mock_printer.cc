// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_printer.h"

namespace mock_printer {

namespace {

class MockPrinterImpl : public MockPrinter {
 public:
  MockPrinterImpl() = default;
  ~MockPrinterImpl() override = default;

  void DoNothing(const SmartBuffer& message) override {}
};

}  // namespace

// static
std::unique_ptr<MockPrinter> MockPrinter::Create() {
  return std::make_unique<MockPrinterImpl>();
}

}  // namespace mock_printer
