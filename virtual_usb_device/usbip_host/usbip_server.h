// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_SERVER_H_
#define VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_SERVER_H_

#include <cstdint>

#include "virtual-usb-printer/virtual_usb_device/server.h"

// Creates a tcp server that listens for USB/IP messages and forwards them to
// the `UsbIpManager` for further processing.
// This class is owned by `UsbIpManager`.
class UsbIpServer : public Server {
 public:
  explicit UsbIpServer(const uint16_t port);

  // Emits message "USBIPHost:READY", so that external entity could know usbip
  // host is ready to process usbip messages.
  void OnServerStarted() final;
};

#endif  // VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_SERVER_H_
