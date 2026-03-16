// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_printer.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <base/check.h>
#include <base/containers/span.h>
#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <chromeos/libipp/ipp_enums.h>

#include "virtual-usb-printer/common/http_util.h"
#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/mock_printer/mock_printer.h"
#include "virtual-usb-printer/mock_printer/parse_textproto.h"
#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/ipp_manager.h"
#include "virtual-usb-printer/virtual_usb_device/ipp_util.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/scanner/scanner.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/usb_device.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"

#define DEFAULT_TCP_PRINTER_PORT 3241
#define DEFAULT_TCP_HOST_PORT 3240

constexpr char kUsage[] =
    "virtual_usb_printer\n"
    "    --descriptors_path=<path>\n"
    "    [--attributes_path=<path>]\n"
    "    [--scanner_capabilities_path=<path>]\n"
    "    [--mock_printer_script=<path>]\n"
    "    [--record_doc_path=<path>]\n"
    "    [--http_header_output_dir=<path>]\n"
    "    --host_port=<unsigned int>\n"
    "    --port=<unsigned int>";

namespace {

using OperationId = ipp::E_operations_supported;

// Creates a `MockPrinter` using the `mocking::TestCase` textproto at
// `script_path`.
std::unique_ptr<mock_printer::MockPrinter> CreateMockPrinter(
    std::string_view script_path) {
  CHECK(!script_path.empty()) << "BUG: expected nonempty `script_path`";

  std::string textproto;
  if (!base::ReadFileToString(base::FilePath(script_path), &textproto)) {
    LOG(ERROR) << "Failed to read textproto " << script_path;
    return nullptr;
  }

  std::optional<mocking::TestCase> script =
      mock_printer::ParseTestCase(textproto);
  if (!script.has_value()) {
    LOG(ERROR) << "Failed to parse `mocking::TestCase` from " << script_path;
    return nullptr;
  }

  return mock_printer::MockPrinter::Create(script.value());
}
}  // namespace

UsbPrinter::UsbPrinter(const UsbDescriptors& usb_descriptors,
                       std::unique_ptr<IppManager> ipp_manager,
                       std::unique_ptr<Scanner> scanner,
                       std::unique_ptr<mock_printer::MockPrinter> mock_printer,
                       const base::FilePath& document_output_path,
                       const base::FilePath& http_output_dir)
    : UsbDevice(usb_descriptors),
      ipp_manager_(std::move(ipp_manager)),
      scanner_(std::move(scanner)),
      mock_printer_(std::move(mock_printer)),
      document_output_path_(document_output_path),
      http_output_dir_(http_output_dir),
      http_ipp_log_counter_(0) {}

std::optional<SmartBuffer> UsbPrinter::HandleUsbData(const Urb& usb_request,
                                                     const SmartBuffer& data) {
  size_t received = data.size();
  LOG(INFO) << "Received " << received << " bytes";
  // Acknowledge receipt of BULK transfer.
  SmartBuffer response;
  response.Add(received);
  SmartBuffer data_buffer = data;
  HandleHttpData(usb_request, &data_buffer);
  return response;
}

void UsbPrinter::HandleHttpData(const Urb& usb_request, SmartBuffer* message) {
  InterfaceManager* im = GetInterfaceManager(usb_request.ep);

  if (!im->receiving_message()) {
    // If we're not currently receiving, `message` must be the start of a new
    // HTTP message. Parse the header and setup some fields to track state.
    std::optional<HttpRequest> opt_request = HttpRequest::Deserialize(message);
    if (!opt_request.has_value()) {
      LOG(ERROR) << "Incoming message is not valid HTTP; ignoring";
      return;
    }
    HttpRequest request = opt_request.value();
    im->set_receiving_message(true);
    im->set_request_header(request);
    im->set_receiving_chunked(request.IsChunkedMessage());
  }

  std::optional<SmartBuffer> payload =
      im->AddMessageAndReturnIfComplete(*message);
  if (!payload.has_value()) {
    return;
  }

  LogHttpHeaders(im->request_header());
  HttpResponse response =
      GenerateHttpResponse(im->request_header(), &payload.value());
  QueueHttpResponse(usb_request, response);
}

HttpResponse UsbPrinter::GenerateHttpResponseFromIppManager(SmartBuffer* body) {
  HttpResponse response;
  std::optional<IppHeader> ipp_header = IppHeader::Deserialize(body);
  if (!ipp_header) {
    LOG(ERROR) << "Request does not contain a valid IPP header.";
    response.status = "415 Unsupported Media Type";
    return response;
  }
  if (!RemoveIppAttributes(body)) {
    LOG(ERROR) << "IPP request has malformed attributes section.";
    response.status = "415 Unsupported Media Type";
    return response;
  }

  LogIppHeaders(ipp_header.value());

  response.status = "200 OK";
  response.headers["Content-Type"] = "application/ipp";
  response.body = ipp_manager_->HandleIppRequest(ipp_header.value(), *body);
  return response;
}

HttpResponse UsbPrinter::GenerateHttpResponseForRequest(
    const HttpRequest& request, SmartBuffer* body) {
  LOG(INFO) << request.request_line;
  if (request.method == "POST" && request.uri == "/ipp/print") {
    if (!mock_printer_) {
      return GenerateHttpResponseFromIppManager(body);
    }

    HttpResponse retval = mock_printer_->GenerateHttpResponse(body);

    if (retval.default_response) {
      return GenerateHttpResponseFromIppManager(body);
    }

    return retval;

  } else if (base::StartsWith(request.uri, "/eSCL",
                              base::CompareCase::SENSITIVE)) {
    return scanner_->HandleEsclRequest(request, *body);
  } else {
    LOG(WARNING) << "Method: " << request.method << " URI: " << request.uri;
  }

  HttpResponse response;
  LOG(ERROR) << "Invalid method '" << request.method << "' and/or endpoint '"
             << request.uri << "'";
  response.status = "404 Not Found";
  return response;
}

HttpResponse UsbPrinter::GenerateHttpResponse(const HttpRequest& request,
                                              SmartBuffer* body) {
  return GenerateHttpResponseForRequest(request, body);
}

void UsbPrinter::QueueHttpResponse(const Urb& usb_request,
                                   const HttpResponse& response) {
  SmartBuffer http_message;
  response.Serialize(&http_message);

  LOG(INFO) << "Queueing HTTP response...";
  InterfaceManager* im = GetInterfaceManager(usb_request.ep);
  im->QueueMessage(http_message);
}

void UsbPrinter::LogHttpHeaders(const HttpRequest& request) {
  if (http_output_dir_.empty()) {
    return;
  }

  const std::string filename =
      base::StringPrintf("http-header-%04d.txt", ++http_ipp_log_counter_);
  const base::FilePath filePath = http_output_dir_.Append(filename);
  base::File file(filePath,
                  base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open/create file at " << filePath;
    return;
  }

  const std::string msg =
      base::StringPrintf("%s\n", request.request_line.c_str());
  auto written = file.WriteAtCurrentPos(base::as_byte_span(msg));
  if (!written.has_value() || *written != msg.length()) {
    PLOG(ERROR) << "Failed to write HTTP request line to file at " << filePath;
    return;
  }

  const HttpHeaders& headers = request.headers;
  for (auto it = headers.begin(); it != headers.end(); ++it) {
    const std::string msg =
        base::StringPrintf("%s: %s\n", it->first.c_str(), it->second.c_str());
    auto written = file.WriteAtCurrentPos(base::as_byte_span(msg));
    if (!written.has_value() || *written != msg.length()) {
      PLOG(ERROR) << "Failed to write http headers to file at " << filePath;
      return;
    }
  }
}

void UsbPrinter::LogIppHeaders(const IppHeader& ipp_header) {
  if (http_output_dir_.empty()) {
    return;
  }

  const std::string filename =
      base::StringPrintf("ipp-header-%04d.txt", http_ipp_log_counter_);
  const base::FilePath filePath = http_output_dir_.Append(filename);
  base::File file(filePath,
                  base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open/create file at " << filePath;
    return;
  }

  const OperationId op_id = static_cast<OperationId>(ipp_header.operation_id);

  const std::string msg = base::StringPrintf(
      "Major: %d\nMinor: %d\nOp ID: %s\nRequest ID: %d\n", ipp_header.major,
      ipp_header.minor, ipp::ToString(op_id).c_str(), ipp_header.request_id);

  auto written = file.WriteAtCurrentPos(base::as_byte_span(msg));
  if (!written.has_value() || *written != msg.length()) {
    PLOG(ERROR) << "Failed to write IPP request line to file at " << filePath;
  }
}

int main(int argc, char* argv[]) {
  DEFINE_string(descriptors_path, "", "Path to descriptors JSON file");
  DEFINE_string(attributes_path, "", "Path to ipp attribute JSON file");
  DEFINE_string(scanner_capabilities_path, "",
                "Path to eSCL ScannerCapabilities JSON file");
  DEFINE_string(mock_printer_script, "",
                "Path to `mocking::TestCase` textproto for mock printer");
  DEFINE_string(record_doc_path, "", "Path to file to record document to");
  DEFINE_string(http_header_output_dir, "",
                "Path to the directory for writing out http header info");
  DEFINE_uint32(port, DEFAULT_TCP_PRINTER_PORT,
                "Port on which this device listens to");
  DEFINE_uint32(host_port, DEFAULT_TCP_HOST_PORT,
                "Usbip host port which this is binded");
  DEFINE_string(output_log_dir, "",
                "Path to the directory for writing out log info");

  brillo::FlagHelper::Init(argc, argv, "Virtual USB Printer");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  if (FLAGS_descriptors_path.empty()) {
    LOG(FATAL) << kUsage;
  }

  std::optional<UsbDescriptors> usb_descriptors =
      UsbDescriptors::Create(FLAGS_descriptors_path);
  if (!usb_descriptors.has_value()) {
    LOG(ERROR) << "Unable to create USB descriptors from file "
               << FLAGS_descriptors_path;
    return 1;
  }

  base::FilePath document_output_path(FLAGS_record_doc_path);

  base::FilePath log_dir;
  if (!FLAGS_output_log_dir.empty()) {
    base::FilePath output_log_path(FLAGS_output_log_dir);
    if (!base::DirectoryExists(output_log_path)) {
      LOG(FATAL) << "Directory doesn't exist.";
    }
    log_dir = output_log_path;
  } else {
    log_dir = base::FilePath();
  }

  base::FilePath http_log_dir;
  if (!FLAGS_http_header_output_dir.empty()) {
    base::FilePath http_output_log_path(FLAGS_http_header_output_dir);
    if (!base::DirectoryExists(http_output_log_path)) {
      LOG(FATAL) << "Directory doesn't exist: " << http_output_log_path;
    }
    http_log_dir = http_output_log_path;
  }

  std::unique_ptr<IppManager> ipp_manager =
      IppManager::Create(FLAGS_attributes_path, document_output_path);

  // Escl scanner
  LOG(INFO) << "Scanning is supported";
  std::unique_ptr<Scanner> scanner =
      Scanner::Create(FLAGS_scanner_capabilities_path, log_dir);

  // Mock printing
  LOG(INFO) << "Mock Printing is supported";
  std::unique_ptr<mock_printer::MockPrinter> mock_printer;
  if (!FLAGS_mock_printer_script.empty()) {
    mock_printer = CreateMockPrinter(FLAGS_mock_printer_script);
  }

  UsbPrinter usb_printer(usb_descriptors.value(), std::move(ipp_manager),
                         std::move(scanner), std::move(mock_printer),
                         document_output_path, http_log_dir);
  usb_printer.Run(FLAGS_port, FLAGS_host_port);
}
