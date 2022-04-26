// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_printer.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <base/check.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <chromeos/libipp/ipp_base.h>

#include "libipp_helper.h"
#include "mock_printer/proto/http.pb.h"
#include "proto_to_http.h"
#include "proto_to_libipp.h"
#include "test_case_holder.h"
#include "virtual-usb-printer/smart_buffer.h"
#include "wrapped_matcher.h"

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
      : server_(),
        test_case_holder_(
            TestCaseHolder::Create(test_case, WrappedMatcher::Create())) {}

  ~MockPrinterImpl() override = default;

  HttpResponse GenerateHttpResponse(SmartBuffer* body) override {
    HttpResponse http_response;
    Response response = ReceiveDataAndReact(*body);
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
      return {/*smart_buffer=*/std::nullopt, /*http_properties=*/std::nullopt};
    }

    if (!server_.ReadRequestFrameFrom(message.contents())) {
      LOG(ERROR) << "BUG: failed to read inbound message";
      JoinServerLogs().LogError();
      return {/*smart_buffer=*/std::nullopt, /*http_properties=*/std::nullopt};
    }
    JoinServerLogs().Vlog(2, "`server_` logs after `ReadRequestFrameFrom()`:");

    std::unique_ptr<ipp::Request> request =
        ipp::Request::NewRequest(server_.GetOperationId());
    if (!server_.ParseRequestAndSaveTo(request.get())) {
      LOG(ERROR) << "BUG: failed to parse inbound message";
      JoinServerLogs().LogError();
      return {/*smart_buffer=*/std::nullopt, /*http_properties=*/std::nullopt};
    }
    JoinServerLogs().Vlog(2, "`server_` logs after `ParseRequestAndSaveTo()`:");

    ToString(request.get()).Vlog(1, "received `ipp::Request`:");
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
      ToString(*std::get_if<const ipp::Request*>(&request))
          .LogError("last received `ipp::Request`:");
      return {/*smart_buffer=*/std::nullopt, /*http_properties=*/std::nullopt};
    }

    std::vector<uint8_t> response_buffer;
    std::unique_ptr<ipp::Response> response =
        ToResponse(mock_response->ipp_response());
    if (response) {
      server_.BuildResponseFrom(response.get());

      if (!server_.WriteResponseFrameTo(&response_buffer)) {
        LOG(ERROR) << "`WriteResponseFrameTo()` failed";
        JoinServerLogs().LogError();
        return {/*smart_buffer=*/std::nullopt,
                /*http_properties=*/std::nullopt};
      }
    }

    VLOG(1) << "Sending response.";
    return {SmartBuffer(response_buffer), mock_response->http_properties()};
  }

  // Returns logs from `server_`.
  MultilineLogHolder JoinServerLogs() const {
    std::vector<std::string> messages;
    messages.reserve(server_.GetErrorLog().size());
    for (const ipp::Log& log : server_.GetErrorLog()) {
      messages.push_back(log.message);
    }
    return MultilineLogHolder{.lines = messages};
  }

  ipp::Server server_;
  std::unique_ptr<TestCaseHolder> test_case_holder_;
};

}  // namespace

// static
std::unique_ptr<MockPrinter> MockPrinter::Create(
    const mocking::TestCase& test_case) {
  return std::make_unique<MockPrinterImpl>(test_case);
}

}  // namespace mock_printer
