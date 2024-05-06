// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USB_DEVICE_DEVICE_SERVER_H_
#define VIRTUAL_USB_DEVICE_USB_DEVICE_DEVICE_SERVER_H_

#include <cstdint>
#include <memory>

#include <base/memory/raw_ptr.h>

#include "virtual-usb-printer/virtual_usb_device/connection/connection.h"
#include "virtual-usb-printer/virtual_usb_device/server.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/usb_device.h"

// Creates server which listens on socket for any raw usb command. All the usb
// command are forwarded to `UsbDevice`. Once created, this class also register
// the device information with UsbIpHost.
// This class is owned by UsbDevice class.
class DeviceServer : public Server {
 public:
  // Creates device server object which listens on `port` and
  // registers itself to usbip host server listening on `host_port`.
  DeviceServer(uint16_t port, uint16_t host_port);
  void OnServerStarted() final;

 private:
  // Connects with the usbip host. Returns socket descriptor on the
  // success or -1 upon failure.
  int ConnectToHost();

  // Registers usb device with the usbip host. Once registered, usb device is
  // assumed to be exported and can be attached by the `usbip attach` command.
  // After attachment, usbip host can interact with the server by sending usb
  // commands.
  // It is called after the server starts listening.
  // Note: The usb device remains connected with the usbip host even after the
  // registration is done. This is needed for the usbip host to track life of
  // the usb device process (connection is closed when device process is dead).
  bool RegisterToHost();

  base::raw_ptr<UsbDevice> device_;
  std::unique_ptr<IConnection> host_connection_;
};

#endif  // VIRTUAL_USB_DEVICE_USB_DEVICE_DEVICE_SERVER_H_
