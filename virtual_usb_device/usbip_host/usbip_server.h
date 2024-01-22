// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_SERVER_H_
#define VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_SERVER_H_

#include <memory>

#include <base/memory/raw_ptr.h>
#include <base/memory/scoped_refptr.h>
#include <base/task/task_runner.h>

#include "virtual-usb-printer/virtual_usb_device/server.h"

class UsbIpManager;

// Creates a tcp server that listens for USB/IP messages and forwards them to
// the `UsbIpManager` for further processing.
// This class is owned by `UsbIpManager`.
class UsbIpServer : public Server {
 public:
  UsbIpServer(UsbIpManager* manager, int port);

  // Process each incoming usbip message on a new thread.
  void HandleConnection(std::unique_ptr<IConnection> conn) final;

  // Emits message "USBIPHost:READY", so that external entity could know usbip
  // host is ready to process usbip messages.
  void OnServerStarted() final;

 private:
  raw_ptr<UsbIpManager> manager_;
};

#endif  // VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_SERVER_H_
