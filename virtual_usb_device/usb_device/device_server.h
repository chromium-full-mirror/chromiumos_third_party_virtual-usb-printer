// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USB_DEVICE_DEVICE_SERVER_H_
#define VIRTUAL_USB_DEVICE_USB_DEVICE_DEVICE_SERVER_H_

#include <base/files/scoped_file.h>
#include <base/memory/raw_ptr.h>

#include "virtual-usb-printer/virtual_usb_device/server.h"

class UsbDevice;

// Creates server which listens on socket for any raw usb command. All the usb
// command are forwarded to `UsbDevice`. Once created, this class also register
// the device information with UsbIpHost.
// This class is owned by UsbDevice class.
class DeviceServer : public Server {
 public:
  // Creates device server object which listens on `port` and
  // registers itself to usbip host server listening on `host_port`.
  DeviceServer(UsbDevice* device, uint16_t port, uint16_t host_port);
  void OnServerStarted() final;

 private:
  // Registers associated usb device with the UsbIpHost. After registeration,
  // UsbIpHost can interact with this server by sending usb commands.
  // After this call, usb device is assumed to be exported and can be attached
  // by `usbip attach` command.
  // This function is called after this server start listening on port.
  bool RegisterToHost();

  // Read messages and forward it to devices for handling.
  void HandleConnection(base::ScopedFD connection) final;

  base::raw_ptr<UsbDevice> device_;
  uint16_t host_port_;
};

#endif  // VIRTUAL_USB_DEVICE_USB_DEVICE_DEVICE_SERVER_H_
