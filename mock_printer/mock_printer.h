// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_MOCK_PRINTER_H_
#define MOCK_PRINTER_MOCK_PRINTER_H_

#include <memory>

#include "mock_printer/proto/control_flow.pb.h"
#include "virtual-usb-printer/common/http_util.h"
#include "virtual-usb-printer/common/smart_buffer.h"

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

  // Creates the response for `UsbPrinter::GenerateHttpResponse()` using the
  // mock printer.
  virtual HttpResponse GenerateHttpResponse(SmartBuffer* body) = 0;
};

}  // namespace mock_printer

#endif  // MOCK_PRINTER_MOCK_PRINTER_H_
