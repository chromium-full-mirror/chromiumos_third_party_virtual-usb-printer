// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_printer.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <base/check.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <chromeos/libipp/builder.h>
#include <chromeos/libipp/frame.h>
#include <chromeos/libipp/parser.h>

#include "libipp_helper.h"
#include "mock_printer/proto/http.pb.h"
#include "proto_to_http.h"
#include "proto_to_libipp.h"
#include "test_case_holder.h"
#include "wrapped_matcher.h"
#include "virtual-usb-printer/common/smart_buffer.h"

namespace mock_printer {

namespace {

// Verbose logging notes
//
// level 1:
//    * Emits logs for every received `ipp::Request` (contingent on
//      successful reading and parsing).
//    * Emits logs for every response sent.
// level 2:
//    * Emits logs for any warnings that appear in the `ipp::Server`
//      log, even if reading and parsing succeed.
class MockPrinterImpl : public MockPrinter {
 public:
  explicit MockPrinterImpl(const mocking::TestCase& test_case)
      : test_case_holder_(
            TestCaseHolder::Create(test_case, WrappedMatcher::Create())) {}

  ~MockPrinterImpl() override = default;

  HttpResponse GenerateHttpResponse(SmartBuffer* body) override {
    HttpResponse http_response;
    Response response = ReceiveDataAndReact(*body);
    if (response.default_response) {
      http_response.default_response = true;
      return http_response;
    }
    if (!response.smart_buffer.has_value()) {
      LOG(ERROR) << "No smart buffer from mock printer.";
      http_response.status = "500 Internal Server Error";
      return http_response;
    }

    http_response.status = ToHttpStatus(response.http_properties);
    http_response.headers["Content-Type"] = "application/ipp";
    http_response.body = response.smart_buffer.value();
    return http_response;
  }

 private:
  // Structured format for the response of the mock printer's internal state
  // machine.
  struct Response {
    // Formatted IPP response.
    std::optional<SmartBuffer> smart_buffer;
    // Used to construct the HTTP response enclosing the IPP response.
    std::optional<mocking::HttpProperties> http_properties;
    // Used to specify if we should use the default response
    bool default_response;
  };

  // Reads `message` and matches it against the contained test case, returning
  // the appropriate response.
  //
  // Returns nullopt if no match was found.
  //
  // TODO(b/205199712): an optional `SmartBuffer` isn't enough to describe all
  // the possible things we might want the mock printer to do (e.g. to stall
  // indefinitely). We'll need to revisit this method to add functionality.
  Response ReceiveDataAndReact(const SmartBuffer& message) {
    if (test_case_holder_->TestCaseFinished()) {
      LOG(ERROR) << "Received unexpected message; test case is finished";
      LOG(ERROR) << "`TestCaseError()`: " << test_case_holder_->TestCaseError();
      return {/*smart_buffer=*/std::nullopt, /*http_properties=*/std::nullopt,
              /*default_response=*/false};
    }

    ipp::SimpleParserLog logs;
    ipp::Frame frame = ipp::Parse(message.data(), message.size(), logs);
    std::unique_ptr<ipp::Frame> request =
        std::make_unique<ipp::Frame>(std::move(frame));
    if (!logs.CriticalErrors().empty()) {
      LOG(ERROR) << "BUG: failed to read inbound message";
      JoinServerLogs(logs).LogError();
      return {/*smart_buffer=*/std::nullopt, /*http_properties=*/std::nullopt,
              /*default_response=*/false};
    }
    JoinServerLogs(logs).Vlog(2, "message parsed by `ipp::Frame`:");

    ToString(*request).Vlog(1, "received `ipp::Frame`:");
    return ProgressAndReact(InputMessage(request.get()));
  }

  // Helper function for `ReceiveDataAndReact()`; given a well-formed
  // input `request`, tries to make progress on the test case, returning
  // the appropriate response.
  Response ProgressAndReact(const InputMessage& request) {
    const mocking::Response* mock_response =
        test_case_holder_->Progress(request);
    if (!mock_response) {
      LOG(ERROR) << "Got nullptr from `TestCaseHolder::Progress()`.";
      LOG(ERROR) << "`TestCaseError()`: " << test_case_holder_->TestCaseError();
      ToString(*std::get<0>(request)).LogError("last received `ipp::Request`:");
      return {/*smart_buffer=*/std::nullopt, /*http_properties=*/std::nullopt,
              /*default_response=*/false};
    }

    if (mock_response->default_response()) {
      return {/*smart_buffer=*/std::nullopt, /*http_properties=*/std::nullopt,
              /*default_response=*/true};
    }

    std::vector<uint8_t> response_buffer;
    std::unique_ptr<ipp::Frame> response =
        ToResponse(mock_response->ipp_response());
    if (response)
      response_buffer = BuildBinaryFrame(*response);

    VLOG(1) << "Sending response.";
    return {SmartBuffer(response_buffer), mock_response->http_properties(),
            false};
  }

  // Returns logs from parsing.
  MultilineLogHolder JoinServerLogs(const ipp::SimpleParserLog& logs) const {
    std::vector<std::string> messages;
    messages.reserve(logs.Errors().size());
    for (const ipp::ParserError& error : logs.Errors()) {
      messages.push_back(ipp::ToString(error));
    }
    return MultilineLogHolder{.lines = messages};
  }

  std::unique_ptr<TestCaseHolder> test_case_holder_;
};

}  // namespace

// static
std::unique_ptr<MockPrinter> MockPrinter::Create(
    const mocking::TestCase& test_case) {
  return std::make_unique<MockPrinterImpl>(test_case);
}

}  // namespace mock_printer
