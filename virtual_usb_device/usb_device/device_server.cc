// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_server.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <base/logging.h>

#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/virtual_usb_device/connection/socket_connection.h"
#include "virtual-usb-printer/virtual_usb_device/op_commands.h"
#include "virtual-usb-printer/virtual_usb_device/usbip_constants.h"

DeviceServer::DeviceServer(uint16_t port, uint16_t host_port)
    : Server(port), host_connection_(new ClientSocketConnection(host_port)) {}

void DeviceServer::OnServerStarted() {
  if (!RegisterToHost()) {
    LOG(ERROR) << "Failed to register with UsbIpHost";
    exit(1);
  } else {
    // This log is used by external enitity to know that usb device is ready to
    // process command. Currently used by `virtual-usb-printer.sh`
    printf("USBDevice::READY\n");
  }
}

bool DeviceServer::RegisterToHost() {
  if (!host_connection_->Start())
    return false;

  BindOrUnbindRequest request;
  request.header.version = kUsbipVersion;
  request.header.command = OP_REQ_BIND_CMD;
  request.port = GetListenPort();

  SmartBuffer buf = PackBindOrUnbindRequest(request);
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

  return true;
}
