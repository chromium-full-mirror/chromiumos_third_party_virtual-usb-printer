// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_RAW_PRINTER_H_
#define VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_RAW_PRINTER_H_

#include <memory>
#include <string>

#include <base/files/file.h>
#include <base/files/file_path.h>

#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/ipp_manager.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/usb_device.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/mock_printer/mock_printer.h"

// Represents a single usb printer that supports only raw data printing.
class RawUsbPrinter : public UsbDevice {
 public:
  // Creates a printer usb device object with provided usb descriptors
  // in `descriptors`. This device registers itself with the usbip
  // host on `host_port` and listens for the usb request on `port`.
  // `document_output_path` is where a document is outputted to mimic printing.
  RawUsbPrinter(const UsbDescriptors& descriptors,
                uint16_t port,
                uint16_t host_port,
                const base::FilePath& document_output_path);

  // Handles usb request by receiving usb data and writing it to file at
  // location `document_output_path_`. This operation is assumed as document
  // printing.
  // It also sends ackowledgement response for amount of data received.
  void HandleUsbData(IConnection* conn, const Urb& usb_request) override;

 private:
  base::FilePath document_output_path_;
};

#endif  // VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_RAW_PRINTER_H_
