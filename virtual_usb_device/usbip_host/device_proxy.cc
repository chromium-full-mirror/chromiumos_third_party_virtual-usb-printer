// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_proxy.h"

#include <arpa/inet.h>
#include <sys/socket.h>

#include <algorithm>
#include <vector>

#include "virtual-usb-printer/virtual_usb_device/server.h"
#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"

std::unique_ptr<DeviceProxy> DeviceProxy::Create(const uint16_t port,
                                                 const ::BusId& id,
                                                 const size_t devnum) {
  std::unique_ptr<DeviceProxy> proxy(new DeviceProxy(port, id, devnum));
  return proxy;
}

DeviceProxy::DeviceProxy(const uint16_t port,
                         const ::BusId& id,
                         const size_t devnum)
    : device_port_(port), busid_(id), devnum_(devnum) {
  ConnectDevice();
}

DeviceProxy::~DeviceProxy() {
  if (is_connected_)
    DisconnectDevice();
  attached_socket_ = -1;
}

bool DeviceProxy::ConnectDevice() {
  connection_ = std::make_unique<ClientSocketConnection>(device_port_);
  bool is_connected_ = connection_->Start();
  if (is_connected_)
    LOG(INFO) << "Connected with usb device on port: " << GetPort();
  else
    LOG(ERROR) << "Failed to connect with usb device on port: " << GetPort();
  return is_connected_;
}

void DeviceProxy::DisconnectDevice() {
  if (!connection_)
    return;
  connection_->Stop();
  is_connected_ = false;
  LOG(INFO) << "Disconnected with usb device on port: " << GetPort();
}

void DeviceProxy::Attach(const IConnection* conn) {
  attached_socket_ = conn->FD();
  LOG(INFO) << "UsbDevice with port:" << GetPort() << " Attached";
}

void DeviceProxy::Detach() {
  attached_socket_ = -1;
  LOG(INFO) << "UsbDevice with port:" << GetPort() << " Detached";
}

bool DeviceProxy::GetOpRepDevInfo(OpRepDevInfo* info) {
  bool stalled = false;
  Urb request;

  request.direction = 0;
  request.ep = 0;
  request.setup =
      0x8006000100001200;  // setup request to get config descriptors
  SmartBuffer descriptors_data;
  if (!HandleUsbDeviceRequest(request, {}, &descriptors_data, &stalled))
    return false;
  UsbDeviceDescriptor usb_descriptors;
  memcpy(&usb_descriptors, descriptors_data.data(),
         sizeof(UsbDeviceDescriptor));

  request.setup =
      0x8006000200000A00;  // setup request to get config descriptors
  SmartBuffer configs_data;
  if (!HandleUsbDeviceRequest(request, {}, &configs_data, &stalled))
    return false;
  UsbConfigurationDescriptor usb_configs;
  memcpy(&usb_configs, configs_data.data(), sizeof(UsbConfigurationDescriptor));
  configs_data.Erase(0, sizeof(UsbConfigurationDescriptor));

  std::vector<UsbInterfaceDescriptor> usb_interfaces;
  while (configs_data.size() > sizeof(UsbInterfaceDescriptor)) {
    UsbInterfaceDescriptor idesc;
    memcpy(&idesc, configs_data.data(), sizeof(UsbInterfaceDescriptor));
    configs_data.Erase(0, sizeof(UsbInterfaceDescriptor));

    usb_interfaces.push_back(idesc);
  }

  CreateOpRepDevInfo(usb_descriptors, usb_configs, usb_interfaces, info);

  // Update busid and usbpath in OpRepDevice
  std::copy(busid_.begin(), busid_.end(), info->device.busID);

  snprintf(info->device.usbPath, sizeof(info->device.usbPath),
           "/sys/devices/pci0000:00/0000:00:01.2/usb1/%d", GetPort());

  info->device.devnum = devnum_;

  return true;
}

bool DeviceProxy::HandleUsbDeviceRequest(const Urb& urb_request,
                                         const SmartBuffer& data,
                                         SmartBuffer* response_data,
                                         bool* stalled) {
  // Send URB to device
  SmartBuffer buf = PackUrb(urb_request);
  buf.Add(data);
  if (!connection_->Send(buf)) {
    is_connected_ = false;
    return is_connected_;
  }

  // Process response of URB
  SmartBuffer reply_buf = connection_->Receive(sizeof(UrbReply));
  if (reply_buf.size() == 0) {
    is_connected_ = false;
    return is_connected_;
  }

  UrbReply result = UnpackUrbReply(&reply_buf);
  if (result.stalled) {
    *stalled = result.stalled;
    return true;
  }

  return ReadUsbResponseData(result.actual_size, response_data);
}

bool DeviceProxy::ReadUsbResponseData(size_t size, SmartBuffer* response_data) {
  SmartBuffer buffer = connection_->Receive(size);
  if (size > 0 && buffer.size() == 0) {
    is_connected_ = false;
    return is_connected_;
  }
  response_data->Add(buffer);

  return true;
}
