// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_SERVER_H_
#define VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_SERVER_H_

#include <base/memory/raw_ptr.h>

#include "virtual-usb-printer/virtual_usb_device/server.h"

class UsbIpManager;

// Create a server which listens for USB/IP messages and forward them
// messages UsbIpManager for further processing.
// This class is owned by `UsbIpManager`.
class UsbIpServer : public Server {
 public:
  UsbIpServer(UsbIpManager* manager, int port);

  // Process each incoming usbip message on a new thread.
  void HandleConnection(base::ScopedFD connection) final;

 private:
  raw_ptr<UsbIpManager> manager_;
};

#endif  // VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_SERVER_H_
