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

DeviceProxy::DeviceProxy(const uint16_t port, const OpRepDevInfo& info)
    : port_(port) {
  device_info_ = info;

  // Setup usb device server address.
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port_);  // port on which usb device listens

  // Convert IPv4 and IPv6 addresses from text to binary.
  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
    LOG(ERROR) << "Invalid address/ Address not supported";
  }
}

bool DeviceProxy::Connect() {
  if ((client_fd_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    LOG(ERROR) << "Socket creation error";
    return false;
  }
  int status =
      connect(client_fd_, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
  if (status < 0) {
    LOG(ERROR) << "Connection Failed";
    return false;
  }
  is_connected_ = true;

  LOG(INFO) << "Connected with device on port: " << port_;

  return true;
}

void DeviceProxy::Disconnect() {
  int status = close(client_fd_);
  if (status < 0) {
    LOG(ERROR) << "Closing connection Failed";
  }
  is_connected_ = false;
}

void DeviceProxy::AttachConnection(int sockfd) {
  attached_socket_ = sockfd;
}

SmartBuffer DeviceProxy::HandleUsbDeviceRequest(
    const UsbipCmdSubmit& usb_request) {
  SmartBuffer buf;
  // TODO(santo):  usb_request need to be packed for network
  buf.Add(usb_request);
  SendBuffer(client_fd_, buf);

  // FIXME(santo): Confirm if size of receivebuffer is ok
  SmartBuffer data = ReceiveBuffer(client_fd_, sizeof(usb_request));
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
