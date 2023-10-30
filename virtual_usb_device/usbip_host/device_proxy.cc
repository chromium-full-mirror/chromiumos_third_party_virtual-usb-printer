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

  return is_connected_;
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

SmartBuffer DeviceProxy::HandleUsbDeviceRequest(const Urb& urb_request) {
  // Send URB to device
  SmartBuffer buf = PackUrb(urb_request);
  SendBuffer(client_fd_, buf);

  // Process response of URB
  UrbReply result;
  SmartBuffer reply_buf = ReceiveBuffer(client_fd_, sizeof(result));
  result = UnpackUrbReply(&reply_buf);

  LOG(INFO) << "Rcv size: " << result.actual_size;

  return ReceiveBuffer(client_fd_, result.actual_size);
}
