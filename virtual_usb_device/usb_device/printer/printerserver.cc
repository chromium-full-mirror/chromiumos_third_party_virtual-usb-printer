// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "virtual-usb-printer/virtual_usb_device/usb_device/printer/printerserver.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <base/check_op.h>
#include <base/logging.h>

#include "virtual-usb-printer/virtual_usb_device/device_request.h"
#include "virtual-usb-printer/virtual_usb_device/op_commands.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/printer/printer.h"

#define TCP_PRINTER_PORT 3241

PrinterServer::PrinterServer(Printer* printer)
    : Server(TCP_PRINTER_PORT), printer_(printer) {}

bool PrinterServer::RegisterToHost() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    LOG(ERROR) << "Socket error, Error: " << strerror(errno);
    return false;
  }
  struct sockaddr_in serv_addr;
  memset(&serv_addr, '0', sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  serv_addr.sin_port = htons(3240);

  int status = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
  if (status < 0) {
    LOG(ERROR) << "Can't connect to host, Error: " << strerror(errno);
    return false;
  }
  BindRequest request;
  request.header.version = 2;
  request.header.command = OP_REQ_BIND_CMD;
  request.device_data.port = TCP_PRINTER_PORT;
  // These are randomly chosen values.
  // TODO(santo): Taken below value from usb descriptors.
  request.device_data.device_type = 1;   /// Type of printer
  request.device_data.device_id = 2345;  //  Id of printer
  SmartBuffer buf = PackBindRequest(request);
  SendBuffer(sock, buf);

  status = close(sock);
  if (status < 0) {
    LOG(WARNING) << "Couldn't close connection to host, Error: "
                 << strerror(errno);
  }
  return true;
}

void PrinterServer::OnServerStarted() {
  if (!RegisterToHost()) {
    LOG(ERROR) << "Failed to register with UsbIpHost";
    exit(1);
  }
}

void PrinterServer::HandleConnection(base::ScopedFD connection) {
  bool connection_open = true;
  while (connection_open) {
    SmartBuffer buffer = ReceiveBuffer(connection.get(), sizeof(uint8_t));
    printer_->HandleRequest(connection.get(), *buffer.data());
  }
}
