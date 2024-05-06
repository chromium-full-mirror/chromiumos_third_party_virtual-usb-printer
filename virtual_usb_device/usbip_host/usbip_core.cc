// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip_core.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include <base/functional/bind.h>

#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/virtual_usb_device/connection/connection.h"
#include "virtual-usb-printer/virtual_usb_device/op_commands.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/virtual_usb_device/usbip.h"
#include "virtual-usb-printer/virtual_usb_device/usbip_constants.h"

namespace {

std::string BusIdToString(const BusId& busid) {
  return std::string(busid.begin(), busid.end());
}

BusId StringToBusId(const std::string& busid) {
  BusId id;
  size_t len = busid.length();
  if (len > id.size())
    LOG(WARNING) << "busid(string) size is larger than expected BusId size";
  size_t to_copy = std::min<size_t>(len, id.size());

  std::fill(id.begin(), id.end(), '\0');
  std::copy(busid.begin(), busid.begin() + to_copy, id.begin());
  return id;
}

}  // namespace

UsbIpCore::UsbIpCore()
    : create_device_proxy_callback_(base::BindRepeating(&DeviceProxy::Create)) {
}

bool UsbIpCore::RegisterDevice(const uint16_t port) {
  const size_t devnum = device_count_ + 1;
  BusId id = {'\0'};
  snprintf(id.data(), id.size(), "1-%zu", devnum);

  std::unique_ptr<DeviceProxy> client =
      create_device_proxy_callback_.Run(port, id, devnum);
  if (!client->ConnectDevice()) {
    LOG(ERROR) << "Failed to connect with usb device on port: " << port;
    return false;
  } else {
    LOG(INFO) << "Connected with usb device on port: " << port;
  }

  proxy_map_[id] = std::move(client);

  device_count_++;

  VLOG(1) << "Device registered: BusId " << BusIdToString(id)
          << ", Device port: " << port << ", device count: " << (devnum);
  return true;
}

bool UsbIpCore::DeregisterDevice(const uint16_t port) {
  const auto it = std::remove_if(
      proxy_map_.begin(), proxy_map_.end(),
      [port](const auto& value) { return (value.second->GetPort() == port); });
  if (it == proxy_map_.end()) {
    LOG(INFO) << "Device with port: " << port << "not found";
    return false;
  }
  proxy_map_.erase(it, proxy_map_.end());
  return true;
}

void UsbIpCore::DetachUsbDevice(int fd) {
  for (auto& [busid, proxy] : proxy_map_) {
    if (proxy->GetAttachedSocket() == fd) {
      proxy->Detach();
      return;
    }
  }
}

// We use port number instead of busid for usbip attach command.
// Thus 32 bytes contains port number as char string, which is conveted
// to port number as integer.
BusId UsbIpCore::ReadBusId(IConnection* conn) {
  BusId id = {'\0'};
  // 32 byte are sent by usbip command for busid information.
  SmartBuffer buf = conn->Receive(32);  // busid array size is 32 byte.
  if (buf.size() == 32) {
    const auto& data = buf.contents();
    std::copy(data.begin(), data.end(), id.begin());
  } else {
    printf("Receive error :%s\n", strerror(errno));
  }

  return id;
}

bool UsbIpCore::HandleAttachInternal(IConnection* conn) {
  int status = -1;
  OpRepImport rep;

  const auto& busid = ReadBusId(conn);
  LOG(INFO) << "Attaching device with busid: " << BusIdToString(busid);

  auto it = proxy_map_.find(busid);
  if (it != proxy_map_.end()) {
    auto& proxy = it->second;
    if (!proxy->IsAttached()) {
      OpRepDevInfo info;
      if (proxy->GetOpRepDevInfo(&info)) {
        proxy->Attach(conn);
        CreateOpRepImport(info, &rep);
        status = 0;  // success
        LOG(INFO) << "USB device on port: " << proxy->GetPort() << " attached";
      } else {
        proxy_map_.erase(it);  // DeviceProxy is invalid, so remove it
      }
    }
  } else {
    LOG(WARNING) << "No device registered with busid: " << BusIdToString(busid);
  }

  SetOpHeader(OP_REP_IMPORT_CMD, status, &rep.header);
  SmartBuffer packed_import = PackOpRepImport(rep);
  conn->Send(packed_import);
  return (status == 0 ? true : false);
}

void UsbIpCore::HandleDeviceListInternal(IConnection* conn) {
  LOG(INFO) << "Listing devices(Active or Inactive)..." << proxy_map_.size();

  OpRepDevlist list{};
  for (auto it = proxy_map_.begin(); it != proxy_map_.end();) {
    auto& proxy = it->second;
    OpRepDevInfo info;
    if (proxy->GetOpRepDevInfo(&info)) {
      list.devices.push_back(info);
      ++it;
    } else {  // DeviceProxy is invalid, so remove it
      it = proxy_map_.erase(it);
    }
  }

  CreateOpRepDevlistHeader(proxy_map_.size(), &list);

  SmartBuffer packed_devlist = PackOpRepDevlist(list);
  conn->Send(packed_devlist);
}

bool UsbIpCore::HandleUsbCommandSubmit(IConnection* conn,
                                       const UsbipCmdSubmit& usb_request) {
  size_t fd = conn->FD();
  // Get DeviceProxy for the device attached so far.
  auto it = std::find_if(proxy_map_.begin(), proxy_map_.end(),
                         [fd](const auto& e) -> bool {
                           return e.second->GetAttachedSocket() == fd;
                         });

  if (it == proxy_map_.end()) {
    return false;
  }

  bool status = HandleUsbCommandSubmitWithProxy(conn, usb_request, it->second);
  if (!status) {  // DeviceProxy is invalid since it lost connection to device.
    proxy_map_.erase(it);
    return false;
  }

  return true;
}

bool UsbIpCore::HandleUsbCommandSubmitWithProxy(
    IConnection* conn,
    const UsbipCmdSubmit& usb_request,
    std::unique_ptr<DeviceProxy>& proxy) {
  Urb urb_request;
  usbip_to_urb(&usb_request, &urb_request);
  LOG(INFO) << "Receive transfer buffer of size: "
            << usb_request.transfer_buffer_length;

  SmartBuffer data;
  // If data transfer is from the host to device.
  if (usb_request.header.direction == 0 /* host to device */ &&
      usb_request.transfer_buffer_length != 0)
    data = conn->Receive(usb_request.transfer_buffer_length);

  SmartBuffer response_data;
  bool stalled = false;
  if (!proxy->HandleUsbDeviceRequest(urb_request, data, &response_data,
                                     &stalled))
    return false;

  if (stalled)
    return true;

  // If data transfer is from the device to host or there are control
  // requests then we have data to read in response.
  if ((usb_request.header.direction == 1 /* device to host */) ||
      (usb_request.header.ep == 0 /* control requests */)) {
    SendUsbipControlResponse(conn, usb_request, response_data.data(),
                             response_data.size());
  } else {
    // Acknowlewdge that usb device have received BULK data.
    size_t size = *(response_data.data());
    memcpy(&size, response_data.data(), response_data.size());
    SendUsbipDataReceivedAck(conn, usb_request, size);
  }

  return true;
}

bool UsbIpCore::IsAttached(const std::string& busId) {
  BusId id = StringToBusId(busId);
  bool is_attached = false;
  if (proxy_map_.find(id) != proxy_map_.end()) {
    is_attached = proxy_map_[id]->IsAttached();
  }
  return is_attached;
}

void UsbIpCore::SendUsbipDataReceivedAck(IConnection* conn,
                                         const UsbipCmdSubmit& usb_request,
                                         size_t received) {
  UsbipRetSubmit response = CreateUsbipRetSubmit(usb_request);
  response.actual_length = received;

  PrintUsbipRetSubmit(response);

  SmartBuffer smart_buffer = PackUsbipRetSubmit(response);
  conn->Send(smart_buffer);
}

void UsbIpCore::SendUsbipControlResponse(IConnection* conn,
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
  conn->Send(smart_buffer);
}
