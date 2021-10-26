// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_printer.h"

#include <base/logging.h>
#include <chromeos/libipp/ipp_base.h>

namespace mock_printer {

namespace {

class MockPrinterImpl : public MockPrinter {
 public:
  explicit MockPrinterImpl(const mocking::TestCase& test_case)
      : server_(), test_case_(test_case) {}
  ~MockPrinterImpl() override = default;

  void ReceiveDataAndReact(const SmartBuffer& message) override {
    const bool result = server_.ReadRequestFrameFrom(message.contents());
    LOG(WARNING) << "mock printer parsing status: " << result;
  }

 private:
  ipp::Server server_;
  mocking::TestCase test_case_;
};

}  // namespace

// static
std::unique_ptr<MockPrinter> MockPrinter::Create(
    const mocking::TestCase& test_case) {
  return std::make_unique<MockPrinterImpl>(test_case);
}

}  // namespace mock_printer
