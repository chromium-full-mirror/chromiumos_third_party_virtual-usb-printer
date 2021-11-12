// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_printer.h"

#include <memory>
#include <string>
#include <vector>

#include <base/check.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <chromeos/libipp/ipp_base.h>

#include "libipp_helper.h"
#include "proto_to_libipp.h"
#include "test_case_holder.h"
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

  base::Optional<SmartBuffer> ReceiveDataAndReact(
      const SmartBuffer& message) override {
    if (test_case_holder_->TestCaseFinished()) {
      LOG(ERROR) << "Received unexpected message; test case is finished";
      LOG(ERROR) << "`TestCaseError()`: " << test_case_holder_->TestCaseError();
      return base::nullopt;
    }

    if (!server_.ReadRequestFrameFrom(message.contents())) {
      LOG(ERROR) << "BUG: failed to read inbound message";
      JoinServerLogs().LogError();
      return base::nullopt;
    }
    JoinServerLogs().Vlog(2, "`server_` logs after `ReadRequestFrameFrom()`:");

    std::unique_ptr<ipp::Request> request =
        ipp::Request::NewRequest(server_.GetOperationId());
    if (!server_.ParseRequestAndSaveTo(request.get())) {
      LOG(ERROR) << "BUG: failed to parse inbound message";
      JoinServerLogs().LogError();
      return base::nullopt;
    }
    JoinServerLogs().Vlog(2, "`server_` logs after `ParseRequestAndSaveTo()`:");

    ToString(request.get()).Vlog(1, "received `ipp::Request`:");
    return ProgressAndReact(InputMessage(request.get()));
  }

 private:
  // Helper function for `ReceiveDataAndReact()`; given a well-formed
  // input `request`, tries to make progress on the test case, returning
  // the appropriate response.
  base::Optional<SmartBuffer> ProgressAndReact(const InputMessage& request) {
    const mocking::Response* mock_response =
        test_case_holder_->Progress(request);
    if (!mock_response) {
      LOG(ERROR) << "Got nullptr from `TestCaseHolder::Progress()`.";
      LOG(ERROR) << "`TestCaseError()`: " << test_case_holder_->TestCaseError();
      ToString(*std::get_if<const ipp::Request*>(&request))
          .LogError("last received `ipp::Request`:");
      return base::nullopt;
    }

    std::unique_ptr<ipp::Response> response =
        ToResponse(mock_response->ipp_response());
    server_.BuildResponseFrom(response.get());

    std::vector<uint8_t> response_buffer;
    if (!server_.WriteResponseFrameTo(&response_buffer)) {
      LOG(ERROR) << "`WriteResponseFrameTo()` failed";
      JoinServerLogs().LogError();
      return base::nullopt;
    }

    VLOG(1) << "Sending response.";
    return SmartBuffer(response_buffer);
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
