// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip_manager.h"

#include <string>
#include <memory>
#include <utility>

#include <base/strings/stringprintf.h>

#include "device_proxy.h"
#include "virtual-usb-printer/virtual_usb_device/server.h"
#include "virtual-usb-printer/virtual_usb_device/op_commands.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/virtual_usb_device/usbip.h"

UsbIpManager::UsbIpManager() : server_(new UsbIpServer(this)) {}

UsbIpManager::~UsbIpManager() {}

void UsbIpManager::Run() {
  server_->Start();
}

void UsbIpManager::HandleBind(const base::ScopedFD& connection) {
  DeviceData devdata;
  ssize_t received = recv(connection.get(), &devdata, sizeof(devdata), 0);
  if (received != sizeof(devdata)) {
    printf("Receive error :%s\n", strerror(errno));
    exit(1);
  }

  UnpackDeviceData(&devdata);
  RegisterDevice(devdata.port, devdata.device_type, devdata.device_id);
}

bool UsbIpManager::HandleOpRequest(const base::ScopedFD& connection,
                                   bool* attached,
                                   bool* connection_open) {
  bool status = false;
  SmartBuffer received = ReceiveBuffer(connection.get(), sizeof(OpHeader));
  if (received.size() != sizeof(OpHeader)) {
    printf("Receive error :%s\n", strerror(errno));
    exit(1);
  }

  OpHeader request = UnpackOpHeader(&received);
  // Custom request (not usbip protocol defined) use to bind the usb device.
  if (request.command == OP_REQ_BIND_CMD) {
    HandleBind(connection);
    *connection_open = false;
  } else if (request.command == OP_REQ_DEVLIST_CMD) {
    status = HandleDeviceList(connection);
    *connection_open = false;
  } else if (request.command == OP_REQ_IMPORT_CMD) {
    HandleAttach(connection);
    *attached = true;
    *connection_open = true;
  }
  return true;
}

bool UsbIpManager::HandleUsbRequest(const base::ScopedFD& connection,
                                    bool* connection_open) {
  SmartBuffer received =
      ReceiveBuffer(connection.get(), sizeof(UsbipCmdSubmit));
  UsbipCmdSubmit command = UnpackUsbipCmdSubmit(&received);
  if (command.header.command == COMMAND_USBIP_CMD_SUBMIT) {
    PrintUsbipCmdSubmit(command);
    HandleUsbCommand(connection.get(), command);
    return true;
  }

  if (command.header.command == COMMAND_USBIP_CMD_UNLINK) {
    LOG(INFO) << "Received unlink URB...ignoring";
    return true;
  }
  return false;
}

void UsbIpManager::HandleConnection(base::ScopedFD connection) {
  bool connection_open = true;
  bool attached = false;

  while (connection_open) {
    if (!attached) {
      HandleOpRequest(connection, &attached, &connection_open);
    } else {
      HandleUsbRequest(connection, &connection_open);
    }
  }
}

void UsbIpManager::RegisterDevice(const uint16_t port,
                                  const uint16_t device_type,
                                  const uint16_t device_id) {
  auto client_ = std::make_unique<DeviceProxy>(port, device_type, device_id);
  if (client_->Connect())
    clients.push_back(std::move(client_));
}

void UsbIpManager::DeregisterDevice(const uint16_t port) {
  for (auto it = clients.begin(); it != clients.end();) {
    if ((*it)->GetPort() == port) {
      (*it)->Disconnect();
      it = clients.erase(it);
    } else {
      ++it;
    }
  }
}

// We use port number instead of busid for usbip attach command.
// Thus 32 bytes contains port number as char string, which is conveted
// to port number as integer.
int UsbIpManager::ReadPort(const base::ScopedFD& sockfd) {
  // 32 byte are sent by usbip command for busid information.
  char busid[32];
  ssize_t received = recv(sockfd.get(), &busid, 32, 0);
  if (received != 32) {
    printf("Receive error :%s\n", strerror(errno));
    exit(1);
  }

  uint16_t port = std::stoi(busid);

  for (size_t i = 0; i < clients.size(); ++i) {
    if (clients[i]->GetPort() == port) {
      return i;
    }
  }
  return -1;
}

void UsbIpManager::HandleAttach(const base::ScopedFD& connection) {
  uint32_t proxy_index = ReadPort(connection);
  if (proxy_index == -1)
    return;

  attachments_[connection.get()] = proxy_index;

  OpRepImport rep;
  if (clients[proxy_index]->IsConnected()) {
    CreateOpRepImport(clients[proxy_index]->device_descriptor(),
                      clients[proxy_index]->configuration_descriptor(), &rep);
  } else {
    SetOpHeader(OP_REP_IMPORT_CMD, 1 /*error*/, &rep.header);
  }

  SmartBuffer packed_import = PackOpRepImport(rep);
  SendBuffer(connection.get(), packed_import);
}

bool UsbIpManager::HandleDeviceList(const base::ScopedFD& connection) {
  LOG(INFO) << "Listing devices..." << clients.size();

  OpRepDevlist list{};
  CreateOpRepDevlistHeader(clients.size(), &list);
  for (size_t i = 0; i < clients.size(); i++) {
    if (clients[i]->IsConnected()) {
      SetOpRepDevInfo(clients[i]->device_descriptor(),
                      clients[i]->configuration_descriptor(),
                      clients[i]->interface_descriptors(), &list.devices[i]);
    }
  }

  SmartBuffer packed_devlist = PackOpRepDevlist(list);
  SendBuffer(connection.get(), packed_devlist);
  return true;
}

void UsbIpManager::HandleUsbCommand(int sockfd,
                                    const UsbipCmdSubmit& usb_request) {
  // Endpoint 0 is used for USB control requests.
  if (usb_request.header.ep == 0) {
    HandleUsbControl(sockfd, usb_request);
  } else {
    /* TODO(santo) : old code, replace with new implementation
    if (usb_request.header.direction == 1) {
      HandleBulkInRequest(sockfd, usb_request);
    } else {
      if (attachments_[sockfd]->IsIPP()) {
        HandleIppUsbData(sockfd, usb_request);
      } else {
        HandleUsbData(sockfd, usb_request);
      }
    }
    */
  }
}

void UsbIpManager::HandleUsbControl(int sockfd,
                                    const UsbipCmdSubmit& usb_request) {
  UsbControlRequest control_request =
      CreateUsbControlRequest(usb_request.setup);
  auto& client = clients[attachments_[sockfd]];
  SmartBuffer buf = client->HandleUsbControl(control_request);
  SendUsbControlResponse(sockfd, usb_request, buf.data(), buf.size());
}
