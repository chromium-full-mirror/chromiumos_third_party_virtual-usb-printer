// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_device.h"

#include <base/check.h>
#include <base/check_op.h>
#include <base/logging.h>
#include <base/notreached.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "virtual-usb-printer/virtual_usb_device/usb_device/device_server.h"
#include "virtual-usb-printer/virtual_usb_device/value_util.h"
#include "virtual-usb-printer/virtual_usb_device/load_config.h"
#include "virtual-usb-printer/virtual_usb_device/usbip_constants.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"

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
    LOG(ERROR) << "Failed to extract usb device configuration as dictionary";
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

UsbDevice::UsbDevice(const std::string& descriptors_path,
                     uint16_t port,
                     uint16_t host_port) {
  auto descriptors = UsbDescriptors::Create(descriptors_path);
  if (!descriptors.has_value()) {
    LOG(FATAL) << "Unable to create USB descriptors from file "
               << descriptors_path;
  }
  usb_descriptors_ = descriptors.value();
  server_.reset(new DeviceServer(this, port, host_port));
}

void UsbDevice::Run() {
  server_->Start();
}

void UsbDevice::HandleUsbRequest(IConnection* conn, const Urb& usb_request) {
  // Endpoint 0 is used for USB control requests.
  if (usb_request.ep == 0) {
    HandleUsbControl(conn, usb_request);
  } else {
    if (usb_request.direction == 1) {
      HandleBulkInRequest(conn, usb_request);
    } else {
      HandleUsbData(conn, usb_request);
    }
  }
}

void UsbDevice::HandleUsbControl(IConnection* conn,
                                 const Urb& usb_request) const {
  UsbControlRequest control_request =
      CreateUsbControlRequest(usb_request.setup);
  int request_type = GetControlType(control_request.bmRequestType);
  switch (request_type) {
    case STANDARD_TYPE:
      HandleStandardControl(conn, usb_request, control_request);
      break;
    case CLASS_TYPE:
      HandleDeviceControl(conn, usb_request, control_request);
      break;
    case VENDOR_TYPE:
    case RESERVED_TYPE:
    default:
      LOG(ERROR) << "Unable to handle request of type: " << request_type;
      break;
  }
}

void UsbDevice::HandleStandardControl(
    IConnection* conn,
    const Urb& usb_request,
    const UsbControlRequest& control_request) const {
  switch (control_request.bRequest) {
    case GET_STATUS:
      HandleGetStatus(conn, usb_request, control_request);
      break;
    case GET_DESCRIPTOR:
      HandleGetDescriptor(conn, usb_request, control_request);
      break;
    case GET_CONFIGURATION:
      HandleGetConfiguration(conn, usb_request, control_request);
      break;
    case CLEAR_FEATURE:
    case SET_FEATURE:
    case SET_ADDRESS:
    case SET_DESCRIPTOR:
    case SET_CONFIGURATION:
    case GET_INTERFACE:
    case SET_INTERFACE:
    case SET_FRAME:
      HandleUnsupportedRequest(conn, usb_request, control_request);
      break;
    default:
      LOG(ERROR) << "Received unknown control request "
                 << unsigned{control_request.bRequest};
      break;
  }
}

void UsbDevice::HandleDeviceControl(
    IConnection* conn,
    const Urb& usb_request,
    const UsbControlRequest& control_request) const {
  switch (control_request.bRequest) {
    case GET_DEVICE_ID:
      HandleGetDeviceId(conn, usb_request, control_request);
      break;
    case GET_PORT_STATUS:
      NOTIMPLEMENTED();
      break;
    case SOFT_RESET:
      NOTIMPLEMENTED();
      break;
    default:
      LOG(ERROR) << "Unknown usb device class request "
                 << unsigned{control_request.bRequest};
  }
}

void UsbDevice::HandleGetStatus(
    IConnection* conn,
    const Urb& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetStatus " << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";
  uint16_t status = 0x1;  // Self-powered.
  SmartBuffer response(sizeof(status));
  response.Add(&status, sizeof(status));
  SendUsbControlResponse(conn, usb_request, response.data(), response.size());
}

void UsbDevice::HandleGetDescriptor(
    IConnection* conn,
    const Urb& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  switch (control_request.wValue1) {
    case USB_DESCRIPTOR_DEVICE:
      HandleGetDeviceDescriptor(conn, usb_request, control_request);
      break;
    case USB_DESCRIPTOR_CONFIGURATION:
      HandleGetConfigurationDescriptor(conn, usb_request, control_request);
      break;
    case USB_DESCRIPTOR_STRING:
      HandleGetStringDescriptor(conn, usb_request, control_request);
      break;
    case USB_DESCRIPTOR_INTERFACE:
    case USB_DESCRIPTOR_ENDPOINT:
    case USB_DESCRIPTOR_DEBUG:
      // Types that are known and not handled.
      HandleUnsupportedRequest(conn, usb_request, control_request);
      break;
    case USB_DESCRIPTOR_DEVICE_QUALIFIER:
      HandleGetDeviceQualifierDescriptor(conn, usb_request, control_request);
      break;
    default:
      LOG(ERROR) << "Unknown descriptor type request: "
                 << unsigned{control_request.wValue1};
      HandleUnsupportedRequest(conn, usb_request, control_request);
  }
}

void UsbDevice::HandleGetDeviceDescriptor(
    IConnection* conn,
    const Urb& usb_request,
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

  SendUsbControlResponse(conn, usb_request, response.data(), response.size());
}

void UsbDevice::HandleGetConfigurationDescriptor(
    IConnection* conn,
    const Urb& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetConfigurationDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(control_request.wLength);
  response.Add(configuration_descriptor());

  if (control_request.wLength == sizeof(configuration_descriptor())) {
    // Only the configuration descriptor itself has been requested.
    VLOG(1) << "Only configuration descriptor requested";
    SendUsbControlResponse(conn, usb_request, response.data(), response.size());
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
  SendUsbControlResponse(conn, usb_request, response.data(), response.size());
}

void UsbDevice::HandleGetDeviceQualifierDescriptor(
    IConnection* conn,
    const Urb& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDeviceQualifierDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(sizeof(qualifier_descriptor()));
  response.Add(qualifier_descriptor());
  SendUsbControlResponse(conn, usb_request, response.data(), response.size());
}

void UsbDevice::HandleGetStringDescriptor(
    IConnection* conn,
    const Urb& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetStringDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  int index = control_request.wValue0;
  const auto& strings = string_descriptors();
  CHECK(index < strings.size());
  SmartBuffer response(strings[index][0]);
  response.Add(strings[index].data(), strings[index][0]);
  SendUsbControlResponse(conn, usb_request, response.data(), response.size());
}

void UsbDevice::HandleGetConfiguration(
    IConnection* conn,
    const Urb& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetConfiguration " << unsigned{control_request.wValue1}
          << "[" << unsigned{control_request.wValue0} << "]";

  // Note: For now we only have one configuration set, so we just respond with
  // with `configuration_descriptor_.bConfigurationValue`.
  const auto& configuration = configuration_descriptor();
  SmartBuffer response(sizeof(configuration.bConfigurationValue));
  response.Add(&configuration.bConfigurationValue,
               sizeof(configuration.bConfigurationValue));
  SendUsbControlResponse(conn, usb_request, response.data(), response.size());
}

void UsbDevice::HandleUnsupportedRequest(
    IConnection* conn,
    const Urb& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleUnsupportedRequest "
          << StandardDeviceRequestString(control_request.bRequest) << ": "
          << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";
  SendUsbControlResponse(conn, usb_request, 0, 0);
}

void UsbDevice::HandleGetDeviceId(
    IConnection* conn,
    const Urb& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDeviceId " << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(ieee_device_id().size());
  response.Add(ieee_device_id());
  SendUsbControlResponse(conn, usb_request, response.data(), response.size());
}

void UsbDevice::HandleUsbData(IConnection* conn, const Urb& usb_request) const {
  NOTIMPLEMENTED();
}

void UsbDevice::HandleBulkInRequest(IConnection* conn, const Urb& usb_request) {
  NOTIMPLEMENTED();
}

void UsbDevice::SendUsbDataResponse(IConnection* conn,
                                    const Urb& usb_request,
                                    size_t received) const {
  NOTIMPLEMENTED();
}

void UsbDevice::SendUsbControlResponse(IConnection* conn,
                                       const Urb& usb_request,
                                       const uint8_t* data,
                                       size_t data_size) const {
  UrbReply reply;
  reply.devid = usb_request.devid;
  reply.direction = usb_request.direction;
  reply.ep = usb_request.ep;
  reply.actual_size = data_size;
  SmartBuffer buf = PackUrbReply(reply);
  if (data_size > 0) {
    buf.Add(data, data_size);
  }
  LOG(INFO) << __func__ << " "
            << "data size: " << data_size;
  conn->Send(buf);
}
