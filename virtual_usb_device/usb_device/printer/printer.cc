// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printer.h"

#include <vector>

#include <base/files/file_util.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "virtual-usb-printer/virtual_usb_device/ipp_util.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/scanner/scanner.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/common/value_util.h"

#include "virtual-usb-printer/mock_printer/mock_printer.h"
#include "virtual-usb-printer/mock_printer/parse_textproto.h"

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

// Appends `buf` to the file at `path`. If no file exists at `path`, it is
// created.
void AppendToFile(base::FilePath path, SmartBuffer buf) {
  base::File file(path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE |
                            base::File::FLAG_APPEND);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open/create file at " << path;
    return;
  }

  int written = file.WriteAtCurrentPos(
      reinterpret_cast<const char*>(buf.data()), buf.size());
  if (written != buf.size()) {
    PLOG(ERROR) << "Failed to write buf to file at " << path;
  }
}

// Creates a `MockPrinter` using the `mocking::TestCase` textproto at
// `script_path`.
std::unique_ptr<mock_printer::MockPrinter> UsbPrinter::CreateMockPrinter(
    base::StringPiece script_path) {
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

UsbPrinter::UsbPrinter(const std::string descriptors_path,
                       const std::string ipp_attributes_path,
                       const std::string& capabilities_path,
                       const std::string& mock_printer_script,
                       uint16_t port,
                       uint16_t host_port,
                       const base::FilePath& document_output_path,
                       const base::FilePath& http_output_dir,
                       base::FilePath log_dir)
    : UsbDevice(descriptors_path, port, host_port),
      document_output_path_(document_output_path),
      http_output_dir_(http_output_dir) {
  // IPP manager
  if (IsIppUsbPrinter() && !ipp_attributes_path.empty()) {
    ipp_manager_ =
        IppManager::Create(ipp_attributes_path, document_output_path_);
  }

  // Escl scanner
  if (SupportScanner()) {
    LOG(INFO) << "Scanning is supported";
    scanner_ = Scanner::Create(capabilities_path, log_dir);
  }

  // Mock printing
  if (!mock_printer_script.empty()) {
    mock_printer_ = CreateMockPrinter(mock_printer_script);
  }
}

void UsbPrinter::HandleRawUsbData(IConnection* conn,
                                  const Urb& usb_request) const {
  SmartBuffer data = conn->Receive(usb_request.transfer_buffer_length);
  size_t received = data.size();
  LOG(INFO) << "Received " << received << " bytes";
  // Acknowledge receipt of BULK transfer.
  SendUsbResponse(conn, usb_request, nullptr, received);
  if (!document_output_path_.empty()) {
    LOG(INFO) << "Recording document...";
    AppendToFile(document_output_path_, data);
  }
}

bool UsbPrinter::IsIppUsbPrinter() const {
  int count = 0;
  const std::vector<UsbInterfaceDescriptor> interfaces =
      interface_descriptors();
  for (const auto& interface : interfaces) {
    // For printer devices bInterfaceClass code is 7, iInterfaceSubClass code
    // is 1 and for IPP based printer bInterfaceProtocol code is 4, as defined
    // by usb device class definition for printing devices
    // (https://www.usb.org/sites/default/files/usbprint11a021811.pdf).
    if (interface.bInterfaceClass == 7 && interface.bInterfaceSubClass == 1 &&
        interface.bInterfaceProtocol == 4) {
      ++count;
    }
  }
  // An ipp-over-usb printer must have at least 2 ipp-over-usb interfaces.
  return count >= 2;
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
  response.status = "200 OK";
  response.headers["Content-Type"] = "application/ipp";
  response.body = ipp_manager_->HandleIppRequest(ipp_header.value(), *body);
  return response;
}

HttpResponse UsbPrinter::GenerateHttpResponse(const HttpRequest& request,
                                              SmartBuffer* body) {
  LOG(INFO) << request.request_line;
  if (request.method == "POST" && request.uri == "/ipp/print") {
    if (mock_printer_) {
      HttpResponse retval = mock_printer_->GenerateHttpResponse(body);
      if (!retval.default_response) {
        return retval;
      }
    }

    if (ipp_manager_)
      return GenerateHttpResponseFromIppManager(body);

  } else if (base::StartsWith(request.uri, "/eSCL",
                              base::CompareCase::SENSITIVE)) {
    return scanner_->HandleEsclRequest(request, *body);
  }

  HttpResponse response;
  LOG(ERROR) << "Invalid method '" << request.method << "' and/or endpoint '"
             << request.uri << "'";
  response.status = "404 Not Found";
  return response;
}

void UsbPrinter::HandleIppUsbData(IConnection* conn, const Urb& usb_request) {
  SmartBuffer message = conn->Receive(usb_request.transfer_buffer_length);
  size_t received = message.size();
  LOG(INFO) << "Received " << received << " bytes";
  // Acknowledge receipt of BULK transfer.
  SendUsbResponse(conn, usb_request, nullptr, received);

  HandleHttpData(usb_request, &message);
}

void UsbPrinter::HandleHttpData(const Urb& usb_request, SmartBuffer* message) {
  InterfaceManager* im = GetInterfaceManager(usb_request.ep);

  if (!im->receiving_message()) {
    // If we're not currently receiving, |message| must be the start of a new
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

void UsbPrinter::QueueHttpResponse(const Urb& usb_request,
                                   const HttpResponse& response) {
  SmartBuffer http_message;
  response.Serialize(&http_message);

  LOG(INFO) << "Queueing HTTP response...";
  InterfaceManager* im = GetInterfaceManager(usb_request.ep);
  im->QueueMessage(http_message);
}

void UsbPrinter::HandleUsbData(IConnection* conn, const Urb& usb_request) {
  if (IsIppUsbPrinter()) {
    HandleIppUsbData(conn, usb_request);
  } else {
    HandleRawUsbData(conn, usb_request);
  }
}

void UsbPrinter::HandleGetStatus(
    IConnection* conn,
    const Urb& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetStatus " << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";
  constexpr uint16_t status = 0x1;  // Self-powered.
  SmartBuffer response(sizeof(status));
  response.Add(&status, sizeof(status));
  SendUsbResponse(conn, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleUnsupportedRequest(
    IConnection* conn,
    const Urb& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleUnsupportedRequest "
          << StandardDeviceRequestString(control_request.bRequest) << ": "
          << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";
  SendUsbResponse(conn, usb_request, nullptr, 0);
}

void UsbPrinter::HandleGetDeviceId(
    IConnection* conn,
    const Urb& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDeviceId " << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(ieee_device_id().size());
  response.Add(ieee_device_id());
  SendUsbResponse(conn, usb_request, response.data(), response.size());
}

void UsbPrinter::LogHttpHeaders(const HttpRequest& request) {
  if (http_output_dir_.empty()) {
    return;
  }

  static int counter = 0;
  const std::string filename =
      base::StringPrintf("http-header-%04d.txt", ++counter);
  const base::FilePath filePath = http_output_dir_.Append(filename);
  base::File file(filePath,
                  base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open/create file at " << filePath;
    return;
  }

  const std::string msg =
      base::StringPrintf("%s\n", request.request_line.c_str());
  const int written = file.WriteAtCurrentPos(msg.c_str(), msg.length());
  if (written != msg.length()) {
    PLOG(ERROR) << "Failed to write HTTP request line to file at " << filePath;
    return;
  }

  const HttpHeaders& headers = request.headers;
  for (auto it = headers.begin(); it != headers.end(); ++it) {
    const std::string msg =
        base::StringPrintf("%s: %s\n", it->first.c_str(), it->second.c_str());
    const int written = file.WriteAtCurrentPos(msg.c_str(), msg.length());
    if (written != msg.length()) {
      PLOG(ERROR) << "Failed to write http headers to file at " << filePath;
      return;
    }
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
  DEFINE_int32(port, DEFAULT_TCP_PRINTER_PORT,
               "Port on which this device listens to");
  DEFINE_int32(host_port, DEFAULT_TCP_HOST_PORT,
               "Usbip host port which this is binded");
  DEFINE_string(output_log_dir, "",
                "Path to the directory for writing out log info");

  brillo::FlagHelper::Init(argc, argv, "Virtual USB Printer");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  if (FLAGS_descriptors_path.empty()) {
    LOG(FATAL) << kUsage;
    return 1;
  }

  base::FilePath document_output_path(FLAGS_record_doc_path);

  base::FilePath log_dir;
  if (!FLAGS_output_log_dir.empty()) {
    base::FilePath output_log_path(FLAGS_output_log_dir);
    if (!base::DirectoryExists(output_log_path)) {
      LOG(FATAL) << "Directory doesn't exist.";
      return 1;
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
      return 1;
    }
    http_log_dir = http_output_log_path;
  }

  UsbPrinter usb_printer(FLAGS_descriptors_path, FLAGS_attributes_path,
                         FLAGS_scanner_capabilities_path,
                         FLAGS_mock_printer_script, FLAGS_port, FLAGS_host_port,
                         document_output_path, http_log_dir, log_dir);
  usb_printer.Run();
}
