// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printer.h"

#include <base/check.h>
#include <base/check_op.h>
#include <base/logging.h>
#include <base/notreached.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "virtual-usb-printer/virtual_usb_device/usb_device/printer/printerserver.h"
#include "virtual-usb-printer/virtual_usb_device/value_util.h"
#include "virtual-usb-printer/virtual_usb_device/load_config.h"
#include "virtual-usb-printer/virtual_usb_device/usbip_constants.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"

constexpr char kUsage[] =
    "virtual_usb_printer\n"
    "    --descriptors_path=<path>\n"
    "    [--record_doc_path=<path>]";

namespace {

// Appends `buf` to the file at `path`. If no file exists at `path`, it is
// created
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

}  // namespace

UsbDescriptors::UsbDescriptors(
    const UsbDeviceDescriptor& device_descriptor,
    const UsbConfigurationDescriptor& configuration_descriptor,
    const UsbDeviceQualifierDescriptor& qualifier_descriptor,
    const std::vector<std::vector<char>>& string_descriptors,
    const std::vector<char>& ieee_device_id,
    const std::vector<UsbInterfaceDescriptor>& interface_descriptors,
    const base::flat_map<uint8_t, std::vector<UsbEndpointDescriptor>>&
        endpoint_descriptors)
    : device_descriptor_(device_descriptor),
      configuration_descriptor_(configuration_descriptor),
      qualifier_descriptor_(qualifier_descriptor),
      string_descriptors_(string_descriptors),
      ieee_device_id_(ieee_device_id),
      interface_descriptors_(interface_descriptors),
      endpoint_descriptors_(endpoint_descriptors) {}

std::optional<UsbDescriptors> UsbDescriptors::Create(
    const std::string& descriptor_file) {
  std::optional<std::string> descriptors_contents =
      GetJSONContents(descriptor_file);
  if (!descriptors_contents.has_value()) {
    LOG(ERROR) << "Failed to load file contents for " << descriptor_file;
    return std::nullopt;
  }

  std::optional<base::Value> descriptors =
      base::JSONReader::Read(*descriptors_contents);
  if (!descriptors) {
    LOG(ERROR) << "Failed to parse " << descriptor_file;
    return std::nullopt;
  }

  if (!descriptors->is_dict()) {
    LOG(ERROR) << "Failed to extract printer configuration as dictionary";
    return std::nullopt;
  }

  UsbDeviceDescriptor device = GetDeviceDescriptor(descriptors->GetDict());
  UsbConfigurationDescriptor configuration =
      GetConfigurationDescriptor(descriptors->GetDict());
  UsbDeviceQualifierDescriptor qualifier =
      GetDeviceQualifierDescriptor(descriptors->GetDict());
  std::vector<UsbInterfaceDescriptor> interfaces =
      GetInterfaceDescriptors(descriptors->GetDict());
  base::flat_map<uint8_t, std::vector<UsbEndpointDescriptor>> endpoint_map =
      GetEndpointDescriptorMap(descriptors->GetDict());
  std::vector<std::vector<char>> strings =
      GetStringDescriptors(descriptors->GetDict());
  std::vector<char> ieee_device_id = GetIEEEDeviceId(descriptors->GetDict());

  return UsbDescriptors(device, configuration, qualifier, strings,
                        ieee_device_id, interfaces, endpoint_map);
}

Printer Printer::Create(int argc, char* argv[]) {
  DEFINE_string(descriptors_path, "", "Path to descriptors JSON file");
  DEFINE_string(record_doc_path, "", "Path to file to record document to");
  brillo::FlagHelper::Init(argc, argv, "Virtual USB Printer");
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  if (FLAGS_descriptors_path.empty()) {
    LOG(FATAL) << kUsage;
  }

  std::optional<UsbDescriptors> usb_descriptors =
      UsbDescriptors::Create(FLAGS_descriptors_path);
  if (!usb_descriptors.has_value()) {
    LOG(FATAL) << "Unable to create USB descriptors from file "
               << FLAGS_descriptors_path;
  }

  base::FilePath document_output_path(FLAGS_record_doc_path);

  Printer printer(usb_descriptors.value(), document_output_path);
  return printer;
}

Printer::Printer(const UsbDescriptors& usb_descriptors,
                 const base::FilePath& document_output_path)
    : usb_descriptors_(usb_descriptors),
      document_output_path_(document_output_path) {
  server_.reset(new PrinterServer(this));
}

void Printer::HandleRequest(int sockfd, const uint32_t request) {
  switch (request) {
    case GET_DEVICE_DESCRIPTOR: {
      SmartBuffer response(sizeof(UsbDeviceDescriptor));
      const UsbDeviceDescriptor& dev = device_descriptor();
      response.Add(dev);
      SendBuffer(sockfd, response);
      break;
    }
    case GET_CONFIG_DESCRIPTOR: {
      SmartBuffer response(sizeof(UsbConfigurationDescriptor));
      const UsbConfigurationDescriptor& dev = configuration_descriptor();
      response.Add(dev);
      SendBuffer(sockfd, response);
      break;
    }
    case GET_QUALIFIER_DESCRIPTOR: {
      SmartBuffer response(sizeof(UsbDeviceQualifierDescriptor));
      const UsbDeviceQualifierDescriptor& dev = qualifier_descriptor();
      response.Add(dev);
      SendBuffer(sockfd, response);
      break;
    }
  }
}

void Printer::HandleUsbRequest(int sockfd, const UsbipCmdSubmit& usb_request) {
  // Endpoint 0 is used for USB control requests.
  if (usb_request.header.ep == 0) {
    HandleUsbControl(sockfd, usb_request);
  } else {
    if (usb_request.header.direction == 1) {
      HandleBulkInRequest(sockfd, usb_request);
    } else {
      HandleUsbData(sockfd, usb_request);
    }
  }
}

void Printer::HandleUsbControl(int sockfd,
                               const UsbipCmdSubmit& usb_request) const {
  UsbControlRequest control_request =
      CreateUsbControlRequest(usb_request.setup);
  int request_type = GetControlType(control_request.bmRequestType);
  switch (request_type) {
    case STANDARD_TYPE:
      HandleStandardControl(sockfd, usb_request, control_request);
      break;
    case CLASS_TYPE:
      HandlePrinterControl(sockfd, usb_request, control_request);
      break;
    case VENDOR_TYPE:
    case RESERVED_TYPE:
    default:
      LOG(ERROR) << "Unable to handle request of type: " << request_type;
      break;
  }
}

void Printer::HandleUsbData(int sockfd,
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

void Printer::HandleStandardControl(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  switch (control_request.bRequest) {
    case GET_STATUS:
      HandleGetStatus(sockfd, usb_request, control_request);
      break;
    case GET_DESCRIPTOR:
      HandleGetDescriptor(sockfd, usb_request, control_request);
      break;
    case GET_CONFIGURATION:
      HandleGetConfiguration(sockfd, usb_request, control_request);
      break;
    case CLEAR_FEATURE:
    case SET_FEATURE:
    case SET_ADDRESS:
    case SET_DESCRIPTOR:
    case SET_CONFIGURATION:
    case GET_INTERFACE:
    case SET_INTERFACE:
    case SET_FRAME:
      HandleUnsupportedRequest(sockfd, usb_request, control_request);
      break;
    default:
      LOG(ERROR) << "Received unknown control request "
                 << unsigned{control_request.bRequest};
      break;
  }
}

void Printer::HandlePrinterControl(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  switch (control_request.bRequest) {
    case GET_DEVICE_ID:
      HandleGetDeviceId(sockfd, usb_request, control_request);
      break;
    case GET_PORT_STATUS:
      NOTIMPLEMENTED();
      break;
    case SOFT_RESET:
      NOTIMPLEMENTED();
      break;
    default:
      LOG(ERROR) << "Unknown printer class request "
                 << unsigned{control_request.bRequest};
  }
}

void Printer::HandleGetStatus(int sockfd,
                              const UsbipCmdSubmit& usb_request,
                              const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetStatus " << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";
  constexpr uint16_t status = 0x1;  // Self-powered.
  SmartBuffer response(sizeof(status));
  response.Add(&status, sizeof(status));
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void Printer::HandleGetDescriptor(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  switch (control_request.wValue1) {
    case USB_DESCRIPTOR_DEVICE:
      HandleGetDeviceDescriptor(sockfd, usb_request, control_request);
      break;
    case USB_DESCRIPTOR_CONFIGURATION:
      HandleGetConfigurationDescriptor(sockfd, usb_request, control_request);
      break;
    case USB_DESCRIPTOR_STRING:
      HandleGetStringDescriptor(sockfd, usb_request, control_request);
      break;
    case USB_DESCRIPTOR_INTERFACE:
    case USB_DESCRIPTOR_ENDPOINT:
    case USB_DESCRIPTOR_DEBUG:
      // Types that are known and not handled.
      HandleUnsupportedRequest(sockfd, usb_request, control_request);
      break;
    case USB_DESCRIPTOR_DEVICE_QUALIFIER:
      HandleGetDeviceQualifierDescriptor(sockfd, usb_request, control_request);
      break;
    default:
      LOG(ERROR) << "Unknown descriptor type request: "
                 << unsigned{control_request.wValue1};
      HandleUnsupportedRequest(sockfd, usb_request, control_request);
  }
}

void Printer::HandleGetDeviceDescriptor(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDeviceDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(sizeof(device_descriptor()));
  const UsbDeviceDescriptor& dev = device_descriptor();

  // If the requested number of bytes is smaller than the size of the device
  // descriptor then only send a portion of the descriptor.
  if (control_request.wLength < sizeof(dev)) {
    response.Add(&dev, control_request.wLength);
  } else {
    response.Add(dev);
  }

  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void Printer::HandleGetConfigurationDescriptor(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetConfigurationDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(control_request.wLength);
  response.Add(configuration_descriptor());

  if (control_request.wLength == sizeof(configuration_descriptor())) {
    // Only the configuration descriptor itself has been requested.
    VLOG(1) << "Only configuration descriptor requested";
    SendUsbControlResponse(sockfd, usb_request, response.data(),
                           response.size());
    return;
  }

  const auto& interfaces = interface_descriptors();
  const auto& endpoints = endpoint_descriptors();

  // Place each interface and their corresponding endnpoint descriptors into the
  // response buffer.
  for (int i = 0; i < configuration_descriptor().bNumInterfaces; ++i) {
    const auto& interface = interfaces[i];
    response.Add(&interface, sizeof(interface));
    auto iter = endpoints.find(interface.bInterfaceNumber);
    if (iter == endpoints.end()) {
      LOG(ERROR) << "Unable to find endpoints for interface "
                 << unsigned{interface.bInterfaceNumber};
      exit(1);
    }
    for (const auto& endpoint : iter->second) {
      response.Add(&endpoint, sizeof(endpoint));
    }
  }

  CHECK_EQ(control_request.wLength, response.size())
      << "Response length does not match requested number of bytes";
  // After filling the buffer with all of the necessary descriptors we can send
  // a response.
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void Printer::HandleGetDeviceQualifierDescriptor(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDeviceQualifierDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(sizeof(qualifier_descriptor()));
  response.Add(qualifier_descriptor());
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void Printer::HandleGetStringDescriptor(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetStringDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  int index = control_request.wValue0;
  const auto& strings = string_descriptors();
  CHECK(index < strings.size());
  SmartBuffer response(strings[index][0]);
  response.Add(strings[index].data(), strings[index][0]);
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void Printer::HandleGetConfiguration(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetConfiguration " << unsigned{control_request.wValue1}
          << "[" << unsigned{control_request.wValue0} << "]";

  // Note: For now we only have one configuration set, so we just respond with
  // with `configuration_descriptor_.bConfigurationValue`.
  const auto& configuration = configuration_descriptor();
  SmartBuffer response(sizeof(configuration.bConfigurationValue));
  response.Add(&configuration.bConfigurationValue,
               sizeof(configuration.bConfigurationValue));
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void Printer::HandleUnsupportedRequest(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleUnsupportedRequest "
          << StandardDeviceRequestString(control_request.bRequest) << ": "
          << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";
  SendUsbControlResponse(sockfd, usb_request, 0, 0);
}

void Printer::HandleGetDeviceId(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDeviceId " << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(ieee_device_id().size());
  response.Add(ieee_device_id());
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void Printer::HandleBulkInRequest(int sockfd,
                                  const UsbipCmdSubmit& usb_request) {}

void Printer::Run() {
  server_->Start();
}

int main(int argc, char* argv[]) {
  Printer usb_printer = Printer::Create(argc, argv);
  usb_printer.Run();
}
