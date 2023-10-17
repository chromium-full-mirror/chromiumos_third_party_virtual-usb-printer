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
    : Server(port), device_(device), host_port_(host_port) {}

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
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in serv_addr;
  memset(&serv_addr, '0', sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  serv_addr.sin_port = htons(host_port_);  // Port on which UsbIpHost listens.

  int status = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
  if (status < 0) {
    LOG(ERROR) << "Can't connect to usbip host";
    return false;
  }

  BindRequest request;
  request.header.version = kUsbipVersion;
  request.header.command = OP_REQ_BIND_CMD;
  request.port = GetListenPort();
  CreateOpRepDevInfo(device_->device_descriptor(),
                     device_->configuration_descriptor(),
                     device_->interface_descriptors(), &request.device_desc);

  SmartBuffer buf = PackBindRequest(request);
  SendBuffer(sock, buf);

  SmartBuffer res = ReceiveBuffer(sock, sizeof(OpHeader));
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

  status = close(sock);
  if (status < 0) {
    LOG(WARNING) << "Couldn't close socket";
  }
  return true;
}
