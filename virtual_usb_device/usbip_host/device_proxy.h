// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USBIP_HOST_DEVICE_PROXY_H_
#define VIRTUAL_USB_DEVICE_USBIP_HOST_DEVICE_PROXY_H_

#include <arpa/inet.h>
#include <sys/socket.h>
#include <memory>
#include <string>
#include <vector>

#include <base/notreached.h>

#include "virtual-usb-printer/virtual_usb_device/connection/connection_factory.h"
#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/op_commands.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/virtual_usb_device/usbip.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/usb_device.h"

// This class act as a proxy for actual usb device similar to
// dbus proxy classes. It communicates with actual usb device via socket and
// receives responses.
// There is single `DeviceProxy` for each usb device registered with UsbIpHost.
class DeviceProxy {
 public:
  // Create a proxy object for usb device which listens on
  // `port` with usb information `info`
  DeviceProxy(const uint16_t port, const BusId& id, const size_t devnum);
  ~DeviceProxy();

  // Connect with usb devices process via socket.
  bool Connect();
  // Close socket connection to usb devices.
  void Disconnect();

  // Connects with the device server and attaches a connection to this proxy.
  // All requests received on the connection will be handled by this proxy.
  void Attach(const IConnection* conn);

  // Disconnect with the device server and removes the attached connection.
  void Detach();

  // Returns the attached socket descriptor.
  int GetAttachedSocket() const { return attached_socket_; }

  // Sends the urb request to the usb device (which runs in different process)
  // for handling and returns the size of response.
  bool HandleUsbDeviceRequest(const Urb& urb_request,
                              const SmartBuffer& data,
                              SmartBuffer* response_data);
  // Returns port number on which usb device is listening.
  uint16_t GetPort() const { return connection_->get_port(); }

  // Returns bus id of device.
  std::string BusId() const { return std::string(device_info_.device.busID); }

  // Returns whether this proxy is connected with actual usb device.
  bool IsConnected() const { return is_connected_; }
  bool IsAttached() const { return (attached_socket_ != -1 ? true : false); }
  bool GetOpRepDevInfo(OpRepDevInfo* info);

 private:
  // FIXME(santo:) Remove this function.
  // Receives `size` amount of response data. The size is determined by the
  // `HandleUsbDeviceRequest` call.
  bool ReadUsbResponseData(const size_t size, SmartBuffer* response_data);

  // connection to device server
  std::unique_ptr<IConnection> connection_;

  bool is_connected_ = false;

  OpRepDevInfo device_info_;

  int attached_socket_ = -1;

  const ::BusId busid_;
  const size_t devnum_;
};
#endif  // VIRTUAL_USB_DEVICE_USBIP_HOST_DEVICE_PROXY_H_
