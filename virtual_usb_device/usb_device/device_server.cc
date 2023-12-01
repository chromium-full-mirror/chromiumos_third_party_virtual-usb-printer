// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_server.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <base/check_op.h>
#include <base/logging.h>

#include "usb_device.h"
#include "virtual-usb-printer/virtual_usb_device/op_commands.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"

DeviceServer::DeviceServer(UsbDevice* device, uint16_t port, uint16_t host_port)
    : Server(port), device_(device),
	  host_connection_(ConnectionFactory::instance().MakeClientConnection(host_port)) {}

void DeviceServer::OnServerStarted() {
  if (!RegisterToHost()) {
    LOG(ERROR) << "Failed to register with UsbIpHost";
    exit(1);
  }
}

void DeviceServer::HandleConnection(base::ScopedFD connection) {
  while (true) {
    SmartBuffer received = ReceiveBuffer(connection.get(), sizeof(Urb));
    if (received.size() != sizeof(Urb)) {
      LOG(INFO) << "Received unknown size data - ignoring it";
      return;
    }

    Urb request = UnpackUrb(&received);
    device_->HandleUsbRequest(connection.get(), request);
  }
}

bool DeviceServer::RegisterToHost() {
  if (!host_connection_->Start())
	  return false;

  BindRequest request;
  request.header.version = kUsbipVersion;
  request.header.command = OP_REQ_BIND_CMD;
  request.port = GetListenPort();
  CreateOpRepDevInfo(device_->device_descriptor(),
                     device_->configuration_descriptor(),
                     device_->interface_descriptors(), &request.device_desc);

  SmartBuffer buf = PackBindRequest(request);
  host_connection_->Send(buf);

  SmartBuffer res = host_connection_->Receive(sizeof(OpHeader));
  if (res.size() != sizeof(OpHeader)) {
    exit(1);  // connection closed before sending all data.
  } else {
    OpHeader response = UnpackOpHeader(&res);
    if (response.status) {
      LOG(ERROR) << "Failed: binding usb device with port: " << request.port;
      exit(1);
    } else {
      LOG(INFO) << "Success:  binding usb device with port: " << request.port;
    }
  }
  host_connection_->Stop();

  return true;
}
