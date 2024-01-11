// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_PRINTER_H_
#define VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_PRINTER_H_

#include <memory>
#include <string>

#include <base/files/file.h>
#include <base/files/file_path.h>

#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/ipp_manager.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/usb_device.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/mock_printer/mock_printer.h"

class Scanner;

// Represents a single usb printer that can handle basic USB control, raw usb
// data, printer-specific USB requests and also responds with the result.
class UsbPrinter : public UsbDevice {
 public:
  // Creates a printer usb device object with config provided by the
  // `descriptors_path`. The `ipp_attribute_path` provides the location of json
  // file containing ipp attributes. This device register itself with the usbip
  // host on `host_port` and listens for the usb request on `port`.
  // `document_output_path` is where a document is outputted to mimic printing.
  UsbPrinter(const std::string descriptors_path,
             const std::string ipp_attributes_path,
             const std::string& capabilities_path,
             const std::string& mock_printer_script,
             uint16_t port,
             uint16_t host_port,
             const base::FilePath& document_output_path,
             base::FilePath log_dir);

  // `UsbDevice` interface implementation for usb printer.
  void HandleGetStatus(IConnection* conn,
                       const Urb& usb_request,
                       const UsbControlRequest& control_request) const override;
  void HandleUsbData(IConnection* conn, const Urb& usb_request) override;

 private:
  std::unique_ptr<mock_printer::MockPrinter> CreateMockPrinter(
      base::StringPiece script_path);
  // Handles printer control specific USB requests.
  void HandleDeviceControl(IConnection* conn,
                           const Urb& usb_request,
                           const UsbControlRequest& control_request) const;

  // Responds with ieee device id of the printer.
  void HandleGetDeviceId(IConnection* conn,
                         const Urb& usb_request,
                         const UsbControlRequest& control_request) const;

  // Used to send an empty response to control requests which are not
  // supported.
  void HandleUnsupportedRequest(IConnection* conn,
                                const Urb& usb_request,
                                const UsbControlRequest& control_request) const;

  // Below functions are used for IPP requests.

  // Receives raw usb data, writes data to the file `record_doc_path` and
  // responds back with the amount of data received. For virtual usb printer,
  // this operation is treated as printing.
  void HandleRawUsbData(IConnection* conn, const Urb& usb_request) const;
  // Receives IPP data and responds with the data received acknowledgment.
  void HandleIppUsbData(IConnection* conn, const Urb& usb_request);
  // Parses http header then aggregates all data chunks to the endpoint
  // interface and generates http response.
  void HandleHttpData(const Urb& usb_request, SmartBuffer* message);
  // Generates http responses for the printing requests identified by
  // `request`.
  HttpResponse GenerateHttpResponse(const HttpRequest& request,
                                    SmartBuffer* body);
  // Uses `IppManager` to handle the printing operation and returns http
  // response.
  HttpResponse GenerateHttpResponseFromIppManager(SmartBuffer* body);
  // Queues http responses to the endpoint interface.
  // Responses are sent back to the user when requested via bulk request.
  void QueueHttpResponse(const Urb& usb_request, const HttpResponse& response);

  // Returns whether this printer supports IPP over USB or not.
  bool IsIppUsbPrinter() const;
  bool SupportScanner() const { return true; }

  // This is always null for non-ipp based printer.
  std::unique_ptr<IppManager> ipp_manager_ = nullptr;
  std::unique_ptr<Scanner> scanner_ = nullptr;

  std::unique_ptr<mock_printer::MockPrinter> mock_printer_;

  base::FilePath document_output_path_;
  base::FilePath http_output_dir_;
};

#endif  // VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_PRINTER_H_
