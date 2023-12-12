// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printer.h"

#include <vector>

#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "virtual-usb-printer/virtual_usb_device/ipp_util.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/virtual_usb_device/value_util.h"

#define DEFAULT_TCP_PRINTER_PORT 3241
#define DEFAULT_TCP_HOST_PORT 3240

constexpr char kUsage[] =
    "virtual_usb_printer\n"
    "    --descriptors_path=<path>\n"
    "    --port=<unsigned int>\n"
    "    --host_port=<unsigned int>\n"
    "    [--record_doc_path=<path>]";

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

UsbPrinter::UsbPrinter(const std::string descriptors_path,
                       const std::string ipp_attributes_path,
                       uint16_t port,
                       uint16_t host_port,
                       const base::FilePath& document_output_path)
    : UsbDevice(descriptors_path, port, host_port),
      document_output_path_(document_output_path) {
  if (IsIppUsbPrinter() && !ipp_attributes_path.empty()) {
    ipp_manager_ =
        IppManager::Create(ipp_attributes_path, document_output_path_);
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
    if (ipp_manager_)
      return GenerateHttpResponseFromIppManager(body);
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

  // TODO(santo): LogHttpHeaders(im->request_header());
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

int main(int argc, char* argv[]) {
  DEFINE_string(descriptors_path, "", "Path to descriptors JSON file");
  DEFINE_string(attributes_path, "", "Path to ipp attribute JSON file");
  DEFINE_string(record_doc_path, "", "Path to file to record document to");
  DEFINE_int32(port, DEFAULT_TCP_PRINTER_PORT,
               "Port on which this device listens to");
  DEFINE_int32(host_port, DEFAULT_TCP_HOST_PORT,
               "Usbip host port which this is binded");

  brillo::FlagHelper::Init(argc, argv, "Virtual USB Printer");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  if (FLAGS_descriptors_path.empty() || FLAGS_record_doc_path.empty()) {
    LOG(FATAL) << kUsage;
  }

  base::FilePath document_output_path(FLAGS_record_doc_path);

  UsbPrinter usb_printer(FLAGS_descriptors_path, FLAGS_attributes_path,
                         FLAGS_port, FLAGS_host_port, document_output_path);
  usb_printer.Run();
}
