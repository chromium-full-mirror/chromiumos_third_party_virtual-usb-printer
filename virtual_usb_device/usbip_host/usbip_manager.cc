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

#define TCP_SERVER_PORT 3240

UsbIpManager::UsbIpManager()
    : server_(new UsbIpServer(this, TCP_SERVER_PORT)) {}

UsbIpManager::~UsbIpManager() {}

void UsbIpManager::Run() {
  server_->Start();
}

void UsbIpManager::RespondBind(const base::ScopedFD& connection, bool status) {
  OpHeader response;
  SetOpHeader(OP_REP_BIND_CMD, (status ? 0 : 1 /*error*/), &response);
  SmartBuffer res = PackOpHeader(response);
  SendBuffer(connection.get(), res);
}

void UsbIpManager::HandleBind(const base::ScopedFD& connection) {
  uint16_t port = 0;
  SmartBuffer port_buffer = ReceiveBuffer(connection.get(), sizeof(port));
  if (port_buffer.size() != sizeof(port)) {
    LOG(ERROR) << "Failed reading port value";
    RespondBind(connection, false);
    return;
  }
  memcpy(&port, port_buffer.data(), sizeof(uint16_t));
  port = ntohs(port);

  OpRepDevInfo device_info;
  SmartBuffer device_buffer =
      ReceiveBuffer(connection.get(), sizeof(OpRepDevice));
  if (device_buffer.size() != sizeof(device_info.device)) {
    LOG(ERROR) << "Failed reading devinfo value";
    RespondBind(connection, false);
    return;
  }
  device_info.device = UnpackOpRepDevice(&device_buffer);

  const size_t interface_size =
      sizeof(OpRepDeviceInterface) * device_info.device.bNumInterfaces;
  SmartBuffer interface_buffer =
      ReceiveBuffer(connection.get(), interface_size);
  if (interface_buffer.size() != interface_size) {
    LOG(ERROR) << "Failed reading devinfo value";
    RespondBind(connection, false);
    return;
  }
  device_info.interfaces =
      reinterpret_cast<OpRepDeviceInterface*>(malloc(interface_size));
  for (size_t i = 0; i < device_info.device.bNumInterfaces; ++i) {
    *(device_info.interfaces + i) =
        UnpackOpRepDeviceInterface(&interface_buffer);
  }

  RespondBind(connection, true);

  UpdateDeviceParameter(&device_info.device);
  RegisterDevice(port, device_info);
}

void UsbIpManager::UpdateDeviceParameter(OpRepDevice* device) {
  snprintf(device->busID, sizeof(device->busID), "1-%zu", (device_count + 1));

  snprintf(device->usbPath, sizeof(device->usbPath),
           "/sys/devices/pci0000:00/0000:00:01.2/usb1/%s", device->busID);

  device->devnum = device_count + 1;
}

bool UsbIpManager::HandleOpRequest(const base::ScopedFD& connection,
                                   bool* attached) {
  SmartBuffer buf = ReceiveBuffer(connection.get(), sizeof(OpHeader));
  if (buf.size() != sizeof(OpHeader)) {
    LOG(INFO) << "An unrecognised message was received - ignoring it";
    return false;
  }

  OpHeader request = UnpackOpHeader(&buf);
  if (request.command == OP_REQ_BIND_CMD) {
    HandleBind(connection);
    return false;
  }

  if (request.command == OP_REQ_DEVLIST_CMD) {
    HandleDeviceList(connection);
    return false;
  }

  if (request.command == OP_REQ_IMPORT_CMD) {
    *attached = HandleAttach(connection);
    return *attached;
  }

  LOG(WARNING) << "Unknown usbip command";

  return false;
}

bool UsbIpManager::HandleUsbRequest(const base::ScopedFD& connection,
                                    bool* connection_open) {
  SmartBuffer received =
      ReceiveBuffer(connection.get(), sizeof(UsbipCmdSubmit));
  if (received.size() != sizeof(UsbipCmdSubmit))
    return false;

  UsbipCmdSubmit command = UnpackUsbipCmdSubmit(&received);
  if (command.header.command == COMMAND_USBIP_CMD_SUBMIT) {
    PrintUsbipCmdSubmit(command);
    HandleUsbCommand(connection, command);
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
      connection_open = HandleOpRequest(connection, &attached);
    } else {
      connection_open = HandleUsbRequest(connection, &connection_open);
    }
  }
}

void UsbIpManager::RegisterDevice(const uint16_t port,
                                  const OpRepDevInfo& device_info) {
  auto client_ = std::make_unique<DeviceProxy>(port, device_info);
  proxy_list.push_back(std::move(client_));
  LOG(INFO) << "USB Device (port:" << port << ")"
            << "registered";
  device_count++;
}

// TODO(santo) : Need to figure out when and how this should be called.
// There is no detach/unbind USB/IP message for now. Probably
// listening to udev event and calling this will be better.
void UsbIpManager::DeregisterDevice(const uint16_t port) {
  for (auto it = proxy_list.begin(); it != proxy_list.end();) {
    if ((*it)->GetPort() == port) {
      (*it)->Disconnect();
      it = proxy_list.erase(it);
    } else {
      ++it;
    }
  }
}

// We use port number instead of busid for usbip attach command.
// Thus 32 bytes contains port number as char string, which is conveted
// to port number as integer.
int UsbIpManager::ReadBusId(const base::ScopedFD& sockfd) {
  // 32 byte are sent by usbip command for busid information.
  SmartBuffer buf =
      ReceiveBuffer(sockfd.get(), 32);  // busid array size is 32 byte.
  if (buf.size() != 32) {
    printf("Receive error :%s\n", strerror(errno));
    return -1;
  }

  std::string busid(reinterpret_cast<const char*>(buf.data()));

  LOG(INFO) << "BusID : " << busid;

  for (size_t i = 0; i < proxy_list.size(); ++i) {
    if (busid.compare(proxy_list[i]->BusId()) == 0) {
      return i;
    }
  }
  return -1;
}

bool UsbIpManager::HandleAttach(const base::ScopedFD& connection) {
  OpRepImport rep;
  int status = -1;

  int proxy_index = ReadBusId(connection);
  LOG(INFO) << "Device proxy index: " << proxy_index;

  if (proxy_index != -1) {
    auto& device = proxy_list[proxy_index];
    if (!device->IsConnected() && device->Connect()) {
      device->AttachConnection(connection.get());
      CreateOpRepImport(device->GetOpRepDevInfo(), &rep);
      status = 0;
    }
  }
  SetOpHeader(OP_REP_IMPORT_CMD, status, &rep.header);

  SmartBuffer packed_import = PackOpRepImport(rep);
  SendBuffer(connection.get(), packed_import);
  return status == 0 ? true : false;
}

void UsbIpManager::HandleDeviceList(const base::ScopedFD& connection) {
  LOG(INFO) << "Listing devices..." << proxy_list.size();

  OpRepDevlist list{};
  CreateOpRepDevlistHeader(proxy_list.size(), &list);
  for (size_t i = 0; i < proxy_list.size(); i++) {
    list.devices.push_back(proxy_list[i]->GetOpRepDevInfo());
  }

  SmartBuffer packed_devlist = PackOpRepDevlist(list);
  SendBuffer(connection.get(), packed_devlist);
}

void UsbIpManager::HandleUsbCommand(const base::ScopedFD& sockfd,
                                    const UsbipCmdSubmit& usb_request) {
  for (auto& proxy : proxy_list) {
    if (proxy->GetAttachedSocket() == sockfd.get()) {
      Urb urb_request;
      usbip_to_urb(&usb_request, &urb_request);
      SmartBuffer buf = proxy->HandleUsbDeviceRequest(urb_request);
      SendUsbipControlResponse(sockfd, usb_request, buf.data(), buf.size());
    }
  }
}

void UsbIpManager::SendUsbipControlResponse(const base::ScopedFD& connection,
                                            const UsbipCmdSubmit& usb_request,
                                            const uint8_t* data,
                                            size_t data_size) {
  UsbipRetSubmit response = CreateUsbipRetSubmit(usb_request);
  response.actual_length = data_size;

  PrintUsbipRetSubmit(response);
  SmartBuffer smart_buffer = PackUsbipRetSubmit(response);
  if (data_size > 0) {
    smart_buffer.Add(data, data_size);
  }
  SendBuffer(connection.get(), smart_buffer);
}
