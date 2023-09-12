// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USBIP_HOST_DEVICE_PROXY_H_
#define VIRTUAL_USB_DEVICE_USBIP_HOST_DEVICE_PROXY_H_

#include <arpa/inet.h>
#include <sys/socket.h>
#include <vector>

#include <base/notreached.h>

#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"

// This class act as a proxy for actual usb device similar to
// dbus proxy classes. It communicates with actual usb device via socket and
// reply user with responses. There is single DeviceProxy for each usb device
// registered with UsbIpHost.
class DeviceProxy {
 public:
  // Takes device listening port number, device type
  // and device id, as params and create Object.
  // TODO(santo): We may need to change it as per requirement.
  DeviceProxy(const uint32_t port, const uint16_t type, const uint32_t id);

  // Connect with usb devices process via socket.
  bool Connect();
  // Close socket connection to usb devices.
  void Disconnect();

  // Handle usb control commands.
  SmartBuffer HandleUsbControl(const UsbControlRequest& control_request);

  // Return the usb device descriptors. Descriptors are obtained by interacting
  // with usb device implementation via socket.
  UsbDeviceDescriptor device_descriptor();
  UsbConfigurationDescriptor configuration_descriptor();
  UsbDeviceQualifierDescriptor qualifier_descriptor();
  std::vector<UsbInterfaceDescriptor> interface_descriptors();

  // Return port number on which usb device is listening.
  uint16_t GetPort() const { return port_; }

  // Return whether this proxy is connected with actual usb device.
  bool IsConnected() const { return is_connected_; }

 private:
  uint32_t port_;
  uint16_t type_;
  uint32_t id_;

  int client_fd_;  // socket descriptor use to talk with device.
  struct sockaddr_in serv_addr;
  bool is_connected_ = false;
};
#endif  // VIRTUAL_USB_DEVICE_USBIP_HOST_DEVICE_PROXY_H_
