// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "raw_printer.h"

#include <optional>

#include <base/containers/span.h>
#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/usb_device.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"

#define DEFAULT_TCP_PRINTER_PORT 3241
#define DEFAULT_TCP_HOST_PORT 3240

constexpr char kUsage[] =
    "virtual_usb_printer\n"
    "    --descriptors_path=<path>\n"
    "    [--record_doc_path=<path>]\n"
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

  auto written =
      file.WriteAtCurrentPos(base::span<const uint8_t>(buf.data(), buf.size()));
  if (!written.has_value() || *written != buf.size()) {
    PLOG(ERROR) << "Failed to write buf to file at " << path;
  }
}

RawUsbPrinter::RawUsbPrinter(const UsbDescriptors& descriptors,
                             const base::FilePath& document_output_path)
    : UsbDevice(descriptors), document_output_path_(document_output_path) {}

std::optional<SmartBuffer> RawUsbPrinter::HandleUsbData(
    const Urb& usb_request, const SmartBuffer& data) {
  SmartBuffer response;
  size_t received = data.size();
  LOG(INFO) << "Received " << received << " bytes";
  // Acknowledge receipt of BULK transfer.
  response.Add(received);
  if (!document_output_path_.empty()) {
    LOG(INFO) << "Recording document...";
    AppendToFile(document_output_path_, data);
  }
  return response;
}

int main(int argc, char* argv[]) {
  DEFINE_string(descriptors_path, "", "Path to descriptors JSON file");
  DEFINE_string(record_doc_path, "", "Path to file to record document to");
  DEFINE_uint32(port, DEFAULT_TCP_PRINTER_PORT,
                "Port on which this device listens to");
  DEFINE_uint32(host_port, DEFAULT_TCP_HOST_PORT,
                "Usbip host port which this is binded");

  brillo::FlagHelper::Init(argc, argv, "Virtual USB Raw Printer");

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

  RawUsbPrinter printer(usb_descriptors.value(), document_output_path);
  printer.Run(FLAGS_port, FLAGS_host_port);
}
