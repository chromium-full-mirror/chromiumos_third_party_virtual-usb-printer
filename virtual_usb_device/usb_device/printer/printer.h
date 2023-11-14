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
#include "virtual-usb-printer/virtual_usb_device/usbip.h"

// Represents a single USB printer that can handle basic USB control and
// printer-specific USB requests and respond with the result.
class UsbPrinter : public UsbDevice {
 public:
  UsbPrinter(const std::string descriptors_path,
             const base::FilePath& document_output_path);

  // Return status of printer.
  void HandleGetStatus(int sockfd,
                       const UsbipCmdSubmit& usb_request,
                       const UsbControlRequest& control_request) const override;

  // Handle usb data received on `sockfd`.
  void HandleUsbData(int sockfd,
                     const UsbipCmdSubmit& usb_request) const override;

 private:
  // Handles printer control specific USB requests.
  void HandleDeviceControl(int sockfd,
                           const UsbipCmdSubmit& usb_request,
                           const UsbControlRequest& control_request) const;

  // Responds with ieee device id of the printer.
  void HandleGetDeviceId(int sockfd,
                         const UsbipCmdSubmit& usb_request,
                         const UsbControlRequest& control_request) const;

  // Used to send an empty response to control requests which are not supported.
  void HandleUnsupportedRequest(int sockfd,
                                const UsbipCmdSubmit& usb_request,
                                const UsbControlRequest& control_request) const;

  base::FilePath document_output_path_;
  base::FilePath http_output_dir_;
};

#endif  // VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_PRINTER_H_
