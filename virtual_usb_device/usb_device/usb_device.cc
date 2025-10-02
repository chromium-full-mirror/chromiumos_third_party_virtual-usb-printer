// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_device.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <base/check.h>
#include <base/containers/flat_map.h>
#include <base/json/json_reader.h>
#include <base/logging.h>
#include <base/notimplemented.h>
#include <base/strings/string_util.h>

#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/common/value_util.h"
#include "virtual-usb-printer/virtual_usb_device/connection/connection.h"
#include "virtual-usb-printer/virtual_usb_device/load_config.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/device_server.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/virtual_usb_device/usbip_constants.h"

void InterfaceManager::QueueMessage(const SmartBuffer& message) {
  queue_.push(message);
}

bool InterfaceManager::QueueEmpty() const {
  return queue_.empty();
}

std::optional<SmartBuffer> InterfaceManager::AddMessageAndReturnIfComplete(
    const SmartBuffer& message) {
  message_.Add(message);
  const bool complete =
      receiving_chunked_ ? ContainsFinalChunk(message)
                         : message_.size() == request_header_.ContentLength();
  if (!complete) {
    return std::nullopt;
  }

  SmartBuffer payload;
  if (receiving_chunked_) {
    // Assemble the chunks into the HTTP response body.
    payload = MergeDocument(&message_);
  } else {
    payload = message_;
    message_.Erase(0, message_.size());
  }

  receiving_message_ = false;
  return payload;
}

SmartBuffer InterfaceManager::PopMessage() {
  CHECK(!QueueEmpty()) << "Can't pop message from empty queue.";
  auto message = queue_.front();
  queue_.pop();
  return message;
}

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

std::optional<UsbDescriptors> UsbDescriptors::CreateFromJson(
    const std::string& descriptors_contents) {
  if (descriptors_contents.empty()) {
    LOG(ERROR) << "Empty JSON string for usb descriptors.";
    return std::nullopt;
  }

  std::optional<base::Value> descriptors = base::JSONReader::Read(
      descriptors_contents, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!descriptors) {
    LOG(ERROR) << "Failed to parse descriptors content";
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

std::optional<UsbDescriptors> UsbDescriptors::Create(
    const std::string& descriptor_file) {
  std::optional<std::string> descriptors_contents =
      GetJSONContents(descriptor_file);
  if (!descriptors_contents.has_value()) {
    LOG(ERROR) << "Failed to load file contents for " << descriptor_file;
    return std::nullopt;
  }

  return UsbDescriptors::CreateFromJson(descriptors_contents.value());
}

InterfaceManager* UsbDevice::GetInterfaceManager(int endpoint) {
  CHECK_GT(endpoint, 0) << "Received request on an invalid endpoint";
  // Since each interface contains a pair of in/out endpoints, we perform this
  // conversion in order to retrieve the corresponding interface number.
  // Examples:
  //   endpoints 1 and 2 both map to interface 0.
  //   endpoints 3 and 4 both map to interface 1.
  int index = (endpoint - 1) / 2;
  CHECK_LT(index, interface_managers_.size())
      << "Received request on an invalid endpoint";
  return &interface_managers_[index];
}

UsbDevice::UsbDevice(const UsbDescriptors& descriptors) {
  usb_descriptors_ = std::move(descriptors);
  interface_managers_.resize(usb_descriptors_.interface_descriptors().size());
}

void UsbDevice::Run(const uint16_t port, const uint16_t host_port) {
  device_server_.reset(new DeviceServer(port, host_port));
  device_server_->SetConnectionHandler(this);

  device_server_->Start();
}

void UsbDevice::HandleConnection(std::unique_ptr<IConnection> conn) {
  while (true) {
    SmartBuffer received = conn->Receive(sizeof(Urb));
    if (received.size() == 0 /* connection closed */)
      return;

    if (received.size() != sizeof(Urb)) {
      LOG(WARNING) << "Received unknown size data - ignoring it";
      continue;
    }

    Urb request = UnpackUrb(&received);
    SmartBuffer data;
    // Data request.
    if (request.ep != 0 && request.direction == 0) {
      data = conn->Receive(request.transfer_buffer_length);
    }
    std::optional<SmartBuffer> response = HandleUsbRequest(request, data);
    if (response.has_value())
      SendUsbResponse(conn.get(), request, response.value().data(),
                      response.value().size(), false);
    else
      SendUsbResponse(conn.get(), request, nullptr, 0, true);
  }
}

std::optional<SmartBuffer> UsbDevice::HandleUsbRequest(
    const Urb& usb_request, const SmartBuffer& data) {
  // Endpoint 0 is used for USB control requests.
  if (usb_request.ep == 0) {
    return HandleUsbControl(usb_request);
  } else {
    if (usb_request.direction == 1) {
      return HandleBulkInRequest(usb_request);
    } else {
      return HandleUsbData(usb_request, data);
    }
  }
}

std::optional<SmartBuffer> UsbDevice::HandleUsbControl(
    const Urb& usb_request) const {
  UsbControlRequest control_request =
      CreateUsbControlRequest(usb_request.setup);
  int request_type = GetControlType(control_request.bmRequestType);
  switch (request_type) {
    case STANDARD_TYPE:
      return HandleStandardControl(usb_request, control_request);
    case CLASS_TYPE:
      return HandleDeviceControl(usb_request, control_request);
    case VENDOR_TYPE:
    case RESERVED_TYPE:
    default:
      LOG(ERROR) << "Unable to handle request of type: " << request_type;
      break;
  }
  return SmartBuffer();
}

SmartBuffer UsbDevice::HandleStandardControl(
    const Urb& usb_request, const UsbControlRequest& control_request) const {
  PrintUsbControlRequest(control_request);
  switch (control_request.bRequest) {
    case GET_STATUS:
      return HandleGetStatus(usb_request, control_request);
    case GET_DESCRIPTOR:
      return HandleGetDescriptor(usb_request, control_request);
    case GET_CONFIGURATION:
      return HandleGetConfiguration(usb_request, control_request);
    case CLEAR_FEATURE:
    case SET_FEATURE:
    case SET_ADDRESS:
    case SET_DESCRIPTOR:
    case SET_CONFIGURATION:
    case GET_INTERFACE:
    case SET_INTERFACE:
    case SET_FRAME:
      return HandleUnsupportedRequest(usb_request, control_request);
    default:
      LOG(ERROR) << "Received unknown control request "
                 << unsigned{control_request.bRequest};
      break;
  }
  return SmartBuffer();
}

std::optional<SmartBuffer> UsbDevice::HandleDeviceControl(
    const Urb& usb_request, const UsbControlRequest& control_request) const {
  switch (control_request.bRequest) {
    case GET_DEVICE_ID:
      return HandleGetDeviceId(usb_request, control_request);
    case GET_MAX_LUN:
      return std::nullopt;  // Not responding i.e Stalling the command.
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
  return SmartBuffer();
}

SmartBuffer UsbDevice::HandleGetStatus(
    const Urb& usb_request, const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetStatus " << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";
  uint16_t status = 0x1;  // Self-powered.
  SmartBuffer response(sizeof(status));
  response.Add(&status, sizeof(status));
  return response;
}

SmartBuffer UsbDevice::HandleGetDescriptor(
    const Urb& usb_request, const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  switch (control_request.wValue1) {
    case USB_DESCRIPTOR_DEVICE:
      return HandleGetDeviceDescriptor(usb_request, control_request);
    case USB_DESCRIPTOR_CONFIGURATION:
      return HandleGetConfigurationDescriptor(usb_request, control_request);
    case USB_DESCRIPTOR_STRING:
      return HandleGetStringDescriptor(usb_request, control_request);
    case USB_DESCRIPTOR_INTERFACE:
    case USB_DESCRIPTOR_ENDPOINT:
    case USB_DESCRIPTOR_DEBUG:
      // Types that are known and not handled.
      return HandleUnsupportedRequest(usb_request, control_request);
    case USB_DESCRIPTOR_DEVICE_QUALIFIER:
      return HandleGetDeviceQualifierDescriptor(usb_request, control_request);
    default:
      LOG(ERROR) << "Unknown descriptor type request: "
                 << unsigned{control_request.wValue1};
      return HandleUnsupportedRequest(usb_request, control_request);
  }
}

SmartBuffer UsbDevice::HandleGetDeviceDescriptor(
    const Urb& usb_request, const UsbControlRequest& control_request) const {
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

  return response;
}

SmartBuffer UsbDevice::HandleGetConfigurationDescriptor(
    const Urb& usb_request, const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetConfigurationDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(control_request.wLength);
  response.Add(configuration_descriptor());

  if (control_request.wLength == sizeof(configuration_descriptor())) {
    // Only the configuration descriptor itself has been requested.
    VLOG(1) << "Only configuration descriptor requested";
    return response;
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

  return response;
}

SmartBuffer UsbDevice::HandleGetDeviceQualifierDescriptor(
    const Urb& usb_request, const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDeviceQualifierDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(sizeof(qualifier_descriptor()));
  response.Add(qualifier_descriptor());
  return response;
}

SmartBuffer UsbDevice::HandleGetStringDescriptor(
    const Urb& usb_request, const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetStringDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  int index = control_request.wValue0;
  const auto& strings = string_descriptors();
  CHECK(index < strings.size());
  SmartBuffer response(strings[index][0]);
  response.Add(strings[index].data(), strings[index][0]);
  return response;
}

SmartBuffer UsbDevice::HandleGetConfiguration(
    const Urb& usb_request, const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetConfiguration " << unsigned{control_request.wValue1}
          << "[" << unsigned{control_request.wValue0} << "]";

  // Note: For now we only have one configuration set, so we just respond with
  // with `configuration_descriptor_.bConfigurationValue`.
  const auto& configuration = configuration_descriptor();
  SmartBuffer response(sizeof(configuration.bConfigurationValue));
  response.Add(&configuration.bConfigurationValue,
               sizeof(configuration.bConfigurationValue));
  return response;
}

SmartBuffer UsbDevice::HandleUnsupportedRequest(
    const Urb& usb_request, const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleUnsupportedRequest "
          << StandardDeviceRequestString(control_request.bRequest) << ": "
          << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";
  return SmartBuffer();
}

SmartBuffer UsbDevice::HandleGetDeviceId(
    const Urb& usb_request, const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDeviceId " << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(ieee_device_id().size());
  response.Add(ieee_device_id());
  return response;
}

SmartBuffer UsbDevice::HandleBulkInRequest(const Urb& usb_request) {
  InterfaceManager* im = GetInterfaceManager(usb_request.ep);
  if (im->QueueEmpty()) {
    LOG(ERROR) << "No queued messages, sending empty response.";
    return SmartBuffer();
  }

  SmartBuffer http_message = im->PopMessage();

  const size_t max_size = usb_request.transfer_buffer_length;

  if (http_message.size() > max_size) {
    size_t leftover_size = http_message.size() - max_size;
    SmartBuffer leftover(leftover_size);
    leftover.Add(http_message, max_size);
    http_message.Shrink(max_size);
    im->QueueMessage(leftover);
  }
  SmartBuffer response_buffer;
  response_buffer.Add(http_message);
  return response_buffer;
}

void UsbDevice::SendUsbResponse(IConnection* conn,
                                const Urb& usb_request,
                                const uint8_t* data,
                                size_t data_size,
                                bool is_stalled) const {
  UrbReply reply;
  reply.devid = usb_request.devid;
  reply.direction = usb_request.direction;
  reply.ep = usb_request.ep;
  reply.actual_size = data_size;
  reply.stalled = is_stalled;
  SmartBuffer buf = PackUrbReply(reply);
  if (data && (data_size > 0)) {
    buf.Add(data, data_size);
  }
  conn->Send(buf);
}
