// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_proxy.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <vector>

#include <base/notreached.h>

#include "virtual-usb-printer/virtual_usb_device/server.h"
#include "virtual-usb-printer/virtual_usb_device/device_request.h"
#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"

DeviceProxy::DeviceProxy(const uint32_t port,
                         const uint16_t type,
                         const uint32_t id)
    : port_(port), type_(type), id_(id) {
  LOG(INFO) << "device type " << type_;
  LOG(INFO) << "device id " << id_;
  LOG(INFO) << "port id " << port_;

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port_);

  // Convert IPv4 and IPv6 addresses from text to binary.
  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
    LOG(ERROR) << "Invalid address/ Address not supported";
  }
}

bool DeviceProxy::Connect() {
  if ((client_fd_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    LOG(ERROR) << "\n Socket creation error \n";
    return false;
  }
  int status =
      connect(client_fd_, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
  if (status < 0) {
    LOG(ERROR) << "\nConnection Failed";
    return false;
  }
  is_connected_ = true;

  return true;
}

void DeviceProxy::Disconnect() {
  int status = close(client_fd_);
  if (status < 0) {
    LOG(ERROR) << "\nClosing connection Failed";
  }
  is_connected_ = false;
}

SmartBuffer DeviceProxy::HandleUsbControl(
    const UsbControlRequest& control_request) {
  SmartBuffer buf;
  buf.Add(control_request);
  SendBuffer(client_fd_, buf);

  // FIXME(santo): Confirm control_request.wLength contain the actual values.
  SmartBuffer data = ReceiveBuffer(client_fd_, control_request.wLength);
  return data;
}

UsbDeviceDescriptor DeviceProxy::device_descriptor() {
  SmartBuffer buf;
  uint8_t req = GET_DEVICE_DESCRIPTOR;
  buf.Add(req);
  SendBuffer(client_fd_, buf);
  SmartBuffer data = ReceiveBuffer(client_fd_, sizeof(UsbDeviceDescriptor));
  return UnpackUsbDescriptor(data);
}

UsbConfigurationDescriptor DeviceProxy::configuration_descriptor() {
  // TODO(santo): adjust better once device class is fully ready.
  SmartBuffer buf;
  uint8_t req = GET_CONFIG_DESCRIPTOR;
  buf.Add(req);
  SendBuffer(client_fd_, buf);
  SmartBuffer data =
      ReceiveBuffer(client_fd_, sizeof(UsbConfigurationDescriptor));
  return UnpackConfigDescriptor(data);
}

UsbDeviceQualifierDescriptor DeviceProxy::qualifier_descriptor() {
  // TODO(santo): adjust better once device class is fully ready.
  SmartBuffer buf;
  uint8_t req = GET_QUALIFIER_DESCRIPTOR;
  buf.Add(req);
  SendBuffer(client_fd_, buf);

  SmartBuffer data =
      ReceiveBuffer(client_fd_, sizeof(UsbDeviceQualifierDescriptor));
  return UnpackDeviceQualifierDescriptor(data);
}

std::vector<UsbInterfaceDescriptor> DeviceProxy::interface_descriptors() {
  // TODO(santo): adjust better once device class is fully ready.
  SmartBuffer buf;
  std::vector<UsbInterfaceDescriptor> descriptors;
  uint8_t req = GET_INTERFACE_DESCRIPTOR;
  buf.Add(req);
  SendBuffer(client_fd_, buf);
  SmartBuffer data = ReceiveBuffer(client_fd_, sizeof(UsbInterfaceDescriptor));
  // TODO(santo): data may have multiple interface descriptors and
  // need to handle.
  UsbInterfaceDescriptor descriptor = UnpackInterfaceDescriptors(data);
  descriptors.emplace_back(descriptor);
  return descriptors;
}
