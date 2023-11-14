// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USBIP_HOST_DEVICE_PROXY_H_
#define VIRTUAL_USB_DEVICE_USBIP_HOST_DEVICE_PROXY_H_

#include <arpa/inet.h>
#include <sys/socket.h>
#include <vector>
#include <string>

#include <base/notreached.h>

#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/virtual_usb_device/usbip.h"
#include "virtual-usb-printer/virtual_usb_device/op_commands.h"

// This class act as a proxy for actual usb device similar to
// dbus proxy classes. It communicates with actual usb device via socket and
// receives responses.
// There is single DeviceProxy for each usb device registered with UsbIpHost.
class DeviceProxy {
 public:
  // Create a proxy object for usb device which listens on
  // `port` with usb information `info`
  DeviceProxy(const uint16_t port, const OpRepDevInfo& info);

  // Connect with usb devices process via socket.
  bool Connect();
  // Close socket connection to usb devices.
  void Disconnect();

  // Attach a socket to this proxy. All messages on this socket will be handled
  // by this proxy.
  void AttachConnection(int sockfd);
  int GetAttachedSocket() const { return attached_socket_; }

  SmartBuffer HandleUsbDeviceRequest(const UsbipCmdSubmit& _usb_request);

  // Return the usb device descriptors. Descriptors are obtained by interacting
  // with usb device implementation via socket.
  UsbDeviceDescriptor device_descriptor();
  UsbConfigurationDescriptor configuration_descriptor();
  UsbDeviceQualifierDescriptor qualifier_descriptor();
  std::vector<UsbInterfaceDescriptor> interface_descriptors();

  // Return port number on which usb device is listening.
  uint16_t GetPort() const { return port_; }

  std::string BusId() const { return std::string(device_info_.device.busID); }

  // Return whether this proxy is connected with actual usb device.
  bool IsConnected() const { return is_connected_; }

  OpRepDevInfo GetOpRepDevInfo() { return device_info_; }

 private:
  // port on which actual usb device is listening.
  uint16_t port_;

  int client_fd_;  // socket descriptor used to communicate with device.
  struct sockaddr_in serv_addr;
  bool is_connected_ = false;

  OpRepDevInfo device_info_;

  int attached_socket_ = -1;
};
#endif  // VIRTUAL_USB_DEVICE_USBIP_HOST_DEVICE_PROXY_H_
