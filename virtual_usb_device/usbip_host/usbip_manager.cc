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

UsbIpManager::UsbIpManager(uint16_t port)
    : server_(new UsbIpServer(this, port)) {}

UsbIpManager::~UsbIpManager() {}

void UsbIpManager::Run() {
  server_->Start();
}

void UsbIpManager::RespondBind(IConnection* conn, bool status) {
  OpHeader response;
  SetOpHeader(OP_REP_BIND_CMD, (status ? 0 : 1 /*error*/), &response);
  SmartBuffer res = PackOpHeader(response);
  conn->Send(res);
}

void UsbIpManager::HandleBind(IConnection* conn) {
  uint16_t port = 0;
  SmartBuffer port_buffer = conn->Receive(sizeof(port));
  if (port_buffer.size() != sizeof(port)) {
    LOG(ERROR) << "Failed reading port value";
    RespondBind(conn, false);
    return;
  }
  memcpy(&port, port_buffer.data(), sizeof(uint16_t));
  port = ntohs(port);

  OpRepDevInfo device_info;
  SmartBuffer device_buffer = conn->Receive(sizeof(OpRepDevice));
  if (device_buffer.size() != sizeof(device_info.device)) {
    LOG(ERROR) << "Failed reading devinfo value";
    RespondBind(conn, false);
    return;
  }
  device_info.device = UnpackOpRepDevice(&device_buffer);

  const size_t interface_size =
      sizeof(OpRepDeviceInterface) * device_info.device.bNumInterfaces;
  SmartBuffer interface_buffer = conn->Receive(interface_size);
  if (interface_buffer.size() != interface_size) {
    LOG(ERROR) << "Failed reading devinfo value";
    RespondBind(conn, false);
    return;
  }
  device_info.interfaces =
      reinterpret_cast<OpRepDeviceInterface*>(malloc(interface_size));
  for (size_t i = 0; i < device_info.device.bNumInterfaces; ++i) {
    *(device_info.interfaces + i) =
        UnpackOpRepDeviceInterface(&interface_buffer);
  }

  RespondBind(conn, true);

  UpdateDeviceParameter(&device_info.device);
  RegisterDevice(port, device_info);

  // We need to track when device process is dead, so we call `conn->Receive`.
  // This call will block since there is no data to receive but returns when
  // device process is dead.
  SmartBuffer status_buffer = conn->Receive(sizeof(uint16_t));
  if (status_buffer.size() == 0) {
    DeregisterDevice(port);
  }
}

void UsbIpManager::UpdateDeviceParameter(OpRepDevice* device) {
  snprintf(device->busID, sizeof(device->busID), "1-%zu", (device_count + 1));

  snprintf(device->usbPath, sizeof(device->usbPath),
           "/sys/devices/pci0000:00/0000:00:01.2/usb1/%s", device->busID);

  device->devnum = device_count + 1;
}

bool UsbIpManager::HandleOpRequest(IConnection* conn, bool* attached) {
  SmartBuffer buf = conn->Receive(sizeof(OpHeader));
  if (buf.size() != sizeof(OpHeader)) {
    LOG(INFO) << "An unrecognised message was received - ignoring it";
    return false;
  }

  OpHeader request = UnpackOpHeader(&buf);
  if (request.command == OP_REQ_BIND_CMD) {
    HandleBind(conn);
    return false;
  }

  if (request.command == OP_REQ_DEVLIST_CMD) {
    HandleDeviceList(conn);
    return false;
  }

  if (request.command == OP_REQ_IMPORT_CMD) {
    *attached = HandleAttach(conn);
    return *attached;
  }

  LOG(WARNING) << "Unknown usbip command";

  return false;
}

bool UsbIpManager::HandleUsbRequest(IConnection* conn) {
  SmartBuffer received = conn->Receive(sizeof(UsbipCmdSubmit));
  if (received.size() == 0) {
    // Connection is closed by usbip detach <port> command
    DetachUsbDevice(conn);
    return false;
  }

  if (received.size() != sizeof(UsbipCmdSubmit))
    return false;

  UsbipCmdSubmit command = UnpackUsbipCmdSubmit(&received);
  if (command.header.command == COMMAND_USBIP_CMD_SUBMIT) {
    PrintUsbipCmdSubmit(command);
    HandleUsbCommandSubmit(conn, command);
    return true;
  }

  if (command.header.command == COMMAND_USBIP_CMD_UNLINK) {
    LOG(INFO) << "Received unlink URB...ignoring";
    return true;
  }

  return false;
}

void UsbIpManager::HandleConnection(std::unique_ptr<IConnection> conn) {
  bool connection_open = true;
  bool attached = false;

  while (connection_open) {
    if (!attached) {
      connection_open = HandleOpRequest(conn.get(), &attached);
    } else {
      connection_open = HandleUsbRequest(conn.get());
    }
  }
}

void UsbIpManager::RegisterDevice(const uint16_t port,
                                  const OpRepDevInfo& device_info) {
  auto client = std::make_unique<DeviceProxy>(port, device_info);
  proxy_list.push_back(std::move(client));
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
      if ((*it)->GetAttachedSocket() != -1) {
        LOG(ERROR) << "Detaching of usb device not done";
      }
      it = proxy_list.erase(it);
      device_count--;
      LOG(INFO) << "usb device with port:" << port << " Deregistered";
    } else {
      ++it;
    }
  }
}

void UsbIpManager::DetachUsbDevice(IConnection* conn) {
  for (auto it = proxy_list.begin(); it != proxy_list.end(); ++it) {
    if ((*it)->GetAttachedSocket() == conn->FD()) {
      (*it)->Detach();
      return;
    }
  }
}

// We use port number instead of busid for usbip attach command.
// Thus 32 bytes contains port number as char string, which is conveted
// to port number as integer.
int UsbIpManager::ReadBusId(IConnection* conn) {
  // 32 byte are sent by usbip command for busid information.
  SmartBuffer buf = conn->Receive(32);  // busid array size is 32 byte.
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

bool UsbIpManager::HandleAttach(IConnection* conn) {
  OpRepImport rep;
  int status = -1;

  int proxy_index = ReadBusId(conn);
  LOG(INFO) << "Device proxy index: " << proxy_index;

  if (proxy_index != -1) {
    auto& device = proxy_list[proxy_index];
    if (!device->IsConnected() && device->Attach(conn)) {
      CreateOpRepImport(device->GetOpRepDevInfo(), &rep);
      LOG(INFO) << "USB device on port: " << device->GetPort() << " attached";
      status = 0;  // success
    }
  }

  SetOpHeader(OP_REP_IMPORT_CMD, status, &rep.header);

  SmartBuffer packed_import = PackOpRepImport(rep);
  conn->Send(packed_import);
  return status == 0 ? true : false;
}

void UsbIpManager::HandleDeviceList(IConnection* conn) {
  LOG(INFO) << "Listing devices..." << proxy_list.size();

  OpRepDevlist list{};
  CreateOpRepDevlistHeader(proxy_list.size(), &list);
  for (size_t i = 0; i < proxy_list.size(); i++) {
    list.devices.push_back(proxy_list[i]->GetOpRepDevInfo());
  }

  SmartBuffer packed_devlist = PackOpRepDevlist(list);
  conn->Send(packed_devlist);
}

void UsbIpManager::HandleUsbCommandSubmit(IConnection* conn,
                                          const UsbipCmdSubmit& usb_request) {
  for (auto& proxy : proxy_list) {
    if (proxy->GetAttachedSocket() == conn->FD()) {
      Urb urb_request;
      usbip_to_urb(&usb_request, &urb_request);
      LOG(INFO) << "Receive transfer buffer of size: "
                << usb_request.transfer_buffer_length;
      SmartBuffer data;
      // If data transfer is from the host to device.
      if (usb_request.header.direction == 0 /* host to device */ &&
          usb_request.transfer_buffer_length != 0)
        data = conn->Receive(usb_request.transfer_buffer_length);

      size_t response_buffer_length =
          proxy->HandleUsbDeviceRequest(urb_request, data);

      SmartBuffer buf;
      // If data transfer is from the device to host or there are control
      // requests.
      if ((usb_request.header.direction == 1 /* device to host */) ||
          (usb_request.header.ep == 0 /* control requests */)) {
        buf = proxy->ReceiveResponseBuffer(response_buffer_length);
        SendUsbipControlResponse(conn, usb_request, buf.data(), buf.size());
      } else {
        // Acknowlewdge that usb device have received data.
        SendUsbipDataReceivedAck(conn, usb_request, response_buffer_length);
      }
    }
  }
}

void UsbIpManager::SendUsbipDataReceivedAck(IConnection* conn,
                                            const UsbipCmdSubmit& usb_request,
                                            size_t received) {
  UsbipRetSubmit response = CreateUsbipRetSubmit(usb_request);
  response.actual_length = received;

  LOG(INFO) << "client sent data size: " << usb_request.transfer_buffer_length;
  LOG(INFO) << "device received : " << received;

  PrintUsbipRetSubmit(response);

  SmartBuffer smart_buffer = PackUsbipRetSubmit(response);
  conn->Send(smart_buffer);
}

void UsbIpManager::SendUsbipControlResponse(IConnection* conn,
                                            const UsbipCmdSubmit& usb_request,
                                            const uint8_t* data,
                                            size_t data_size) {
  UsbipRetSubmit response = CreateUsbipRetSubmit(usb_request);
  response.actual_length = data_size;
  LOG(INFO) << "client requested data size: "
            << usb_request.transfer_buffer_length;
  LOG(INFO) << "device receiving : " << data_size;

  PrintUsbipRetSubmit(response);
  SmartBuffer smart_buffer = PackUsbipRetSubmit(response);
  if (data_size > 0) {
    smart_buffer.Add(data, data_size);
  }
  conn->Send(smart_buffer);
}
