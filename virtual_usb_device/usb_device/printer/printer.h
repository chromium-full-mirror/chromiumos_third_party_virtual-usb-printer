// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_PRINTER_H_
#define VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_PRINTER_H_

#include <string>

#include <base/files/file.h>
#include <base/files/file_path.h>

#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/usb_device.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"

// Represents a single usb printer that can handle basic USB control, raw usb
// data, printer-specific USB requests and also responds with the result.
class UsbPrinter : public UsbDevice {
 public:
  // Creates a printer usb device object with config provided by
  // `descriptors_path`. This device register itself with the usbip host
  // on `host_port` and listens for the usb request on `port`.
  // `document_output_path` is where a document is outputted to mimic
  // printing.
  UsbPrinter(const std::string descriptors_path,
             uint16_t port,
             uint16_t host_port,
             const base::FilePath& document_output_path);

  // `UsbDevice` interface implementation for usb printer.
  void HandleGetStatus(IConnection* conn,
                       const Urb& usb_request,
                       const UsbControlRequest& control_request) const override;
  void HandleUsbData(IConnection* conn, const Urb& usb_request) const override;

 private:
  // Handles printer control specific USB requests.
  void HandleDeviceControl(IConnection* conn,
                           const Urb& usb_request,
                           const UsbControlRequest& control_request) const;

  // Responds with ieee device id of the printer.
  void HandleGetDeviceId(IConnection* conn,
                         const Urb& usb_request,
                         const UsbControlRequest& control_request) const;

  // Used to send an empty response to control requests which are not supported.
  void HandleUnsupportedRequest(IConnection* conn,
                                const Urb& usb_request,
                                const UsbControlRequest& control_request) const;

  base::FilePath document_output_path_;
  base::FilePath http_output_dir_;
};

#endif  // VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_PRINTER_H_
