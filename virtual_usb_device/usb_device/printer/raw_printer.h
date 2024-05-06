// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_RAW_PRINTER_H_
#define VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_RAW_PRINTER_H_

#include <optional>

#include <base/files/file_path.h>

#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/usb_device.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"

// Represents a single usb printer that supports only raw data printing.
class RawUsbPrinter : public UsbDevice {
 public:
  // Creates a printer usb device object with provided usb descriptors
  // in `descriptors`.`document_output_path` is where a document is outputted
  // to mimic printing.
  RawUsbPrinter(const UsbDescriptors& descriptors,
                const base::FilePath& document_output_path);

  // Handles usb request by receiving usb data and writing it to file at
  // location `document_output_path_`. This operation is assumed as document
  // printing.
  // It returns the size of data received in optional `SmartBuffer`. If return
  // value doesn't contain any value then it means that the operation was
  // stalled.
  std::optional<SmartBuffer> HandleUsbData(const Urb& usb_request,
                                           const SmartBuffer& data) override;

 private:
  base::FilePath document_output_path_;
};

#endif  // VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_RAW_PRINTER_H_
