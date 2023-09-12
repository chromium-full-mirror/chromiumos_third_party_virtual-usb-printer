
// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_PRINTERSERVER_H_
#define VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_PRINTERSERVER_H_

#include <base/files/scoped_file.h>
#include <base/memory/raw_ptr.h>

#include "virtual-usb-printer/virtual_usb_device/server.h"

class Printer;

// This class creates a socket and listens for any usb command.
// All the usb commands are forwarded to the actual device implementation.
// It also registers the device information with UsbIpHost.
// The instance of this class is owned by Printer object.
// TODO(santo): Break this into generic device server so that it can be used
// with any new usb devices.
class PrinterServer : public Server {
 public:
  explicit PrinterServer(Printer* printer);
  void OnServerStarted() final;

 private:
  // Connect with UsbIpHost and register device information.
  // After this call, usb device is assumed to be exported and can be attached
  // by the user.
  // This should be called when the device server is ready to listen.
  bool RegisterToHost();

  // Read incoming requests and forward them to the printer object.
  void HandleConnection(base::ScopedFD connection) final;

  base::raw_ptr<Printer> printer_;
};

#endif  // VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_PRINTERSERVER_H_
