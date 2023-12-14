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

// This class act as a proxy for actual usb device similar to
// dbus proxy classes. It communicates with actual usb device via socket and
// receives responses.
// There is single `DeviceProxy` for each usb device registered with UsbIpHost.
class DeviceProxy {
 public:
  // Create a proxy object for usb device which listens on
  // `port` with usb information `info`
  DeviceProxy(const uint16_t port, const OpRepDevInfo& info);
  ~DeviceProxy() = default;

  // Connect with usb devices process via socket.
  bool Connect();
  // Close socket connection to usb devices.
  void Disconnect();

  // Connects with the device server and attaches a connection to this proxy.
  // All requests received on the connection will be handled by this proxy.
  bool Attach(IConnection* conn);

  // Disconnect with the device server and removes the attached connection.
  void Detach();

  // Returns the attached socket descriptor.
  int GetAttachedSocket() const { return attached_socket_; }

  // Sends the urb request to the usb device for handling and receives the
  // result which is returned.
  SmartBuffer HandleUsbDeviceRequest(const Urb& urb_request);

  // Returns port number on which usb device is listening.
  uint16_t GetPort() const { return connection_->get_port(); }

  // Returns bus id of device.
  std::string BusId() const { return std::string(device_info_.device.busID); }

  // Returns whether this proxy is connected with actual usb device.
  bool IsConnected() const { return is_connected_; }

  OpRepDevInfo GetOpRepDevInfo() { return device_info_; }

 private:
  // connection to device server
  std::unique_ptr<IConnection> connection_;

  bool is_connected_ = false;

  OpRepDevInfo device_info_;

  int attached_socket_ = -1;
};
#endif  // VIRTUAL_USB_DEVICE_USBIP_HOST_DEVICE_PROXY_H_
