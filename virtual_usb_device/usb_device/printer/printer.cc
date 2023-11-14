// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printer.h"

#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "virtual-usb-printer/virtual_usb_device/usb_util.h"

#define DEFAULT_TCP_PRINTER_PORT 3241
#define DEFAULT_TCP_HOST_PORT 3240

constexpr char kUsage[] =
    "virtual_usb_device\n"
    "    --descriptors_path=<path>\n"
    "    --port=<int>\n"
    "    --host_port=<int>\n"
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
                       const base::FilePath& document_output_path)
    : UsbDevice(descriptors_path, DEFAULT_TCP_PRINTER_PORT),
      document_output_path_(document_output_path) {}

void UsbPrinter::HandleUsbData(int sockfd,
                               const UsbipCmdSubmit& usb_request) const {
  SmartBuffer data = ReceiveBuffer(sockfd, usb_request.transfer_buffer_length);
  size_t received = data.size();
  LOG(INFO) << "Received " << received << " bytes";
  // Acknowledge receipt of BULK transfer.
  SendUsbDataResponse(sockfd, usb_request, received);
  if (!document_output_path_.empty()) {
    LOG(INFO) << "Recording document...";
    AppendToFile(document_output_path_, data);
  }
}

void UsbPrinter::HandleGetStatus(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetStatus " << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";
  constexpr uint16_t status = 0x1;  // Self-powered.
  SmartBuffer response(sizeof(status));
  response.Add(&status, sizeof(status));
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleUnsupportedRequest(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleUnsupportedRequest "
          << StandardDeviceRequestString(control_request.bRequest) << ": "
          << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";
  SendUsbControlResponse(sockfd, usb_request, 0, 0);
}

void UsbPrinter::HandleGetDeviceId(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDeviceId " << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(ieee_device_id().size());
  response.Add(ieee_device_id());
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

int main(int argc, char* argv[]) {
  DEFINE_string(descriptors_path, "", "Path to descriptors JSON file");
  DEFINE_string(record_doc_path, "", "Path to file to record document to");

  brillo::FlagHelper::Init(argc, argv, "Virtual USB Printer");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  if (FLAGS_descriptors_path.empty() || FLAGS_record_doc_path.empty()) {
    LOG(FATAL) << kUsage;
  }

  base::FilePath document_output_path(FLAGS_record_doc_path);

  UsbPrinter usb_printer(FLAGS_descriptors_path, document_output_path);
  usb_printer.Run();
}
