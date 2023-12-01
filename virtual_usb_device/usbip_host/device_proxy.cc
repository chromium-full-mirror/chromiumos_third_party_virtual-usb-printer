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
    : connection_(ConnectionFactory::instance().MakeClientConnection(port)) {
  device_info_ = info;
}

bool DeviceProxy::Connect() {
  is_connected_ = connection_->Start();
  if (is_connected_)
	  LOG(INFO) << "Connected with device on port: " << connection_->get_port();

  return is_connected_;
}

void DeviceProxy::Disconnect() {
  connection_->Stop();
  is_connected_ = false;
  LOG(INFO) << "Disconnected with device on port: " << port_;
}

void DeviceProxy::AttachConnection(int sockfd) {
  attached_socket_ = sockfd;
}

SmartBuffer DeviceProxy::HandleUsbDeviceRequest(const Urb& urb_request) {
  // Send URB to device
  SmartBuffer buf = PackUrb(urb_request);
  connection_->Send(buf);

  // Process response of URB
  UrbReply result;
  SmartBuffer reply_buf = connection_->Receive(sizeof(result));
  result = UnpackUrbReply(&reply_buf);

  LOG(INFO) << "Rcv size: " << result.actual_size;

  return connection_->Receive(result.actual_size);
}
