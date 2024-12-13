// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_USB_PRINTER_H_
#define VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_USB_PRINTER_H_

#include <optional>
#include <memory>

#include <base/files/file_path.h>

#include "virtual-usb-printer/common/http_util.h"
#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/mock_printer/mock_printer.h"
#include "virtual-usb-printer/mock_printer/parse_textproto.h"
#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/ipp_manager.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/scanner/scanner.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/usb_device.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"

// Represents a generic usb device that supports IPP over USB. It could extract
// the IPP message sent over USB and take actions. Any specific device can
// override this class to provide device specific implementation.
class UsbPrinter : public UsbDevice {
 public:
  // Creates a USB printer object where `usb_descriptors` provides usb
  // descriptors, `ipp_manager` handles the ipp requests, `scanner` provides the
  // scanner implemenation and `mock_printer` provides mock printer
  // implementation.
  // `document_output_path` is where a document is outputted to mimic printing
  // and `http_output_dir` tells about directory where http frame logging is
  // done.
  UsbPrinter(const UsbDescriptors& usb_descriptors,
             std::unique_ptr<IppManager> ipp_manager,
             std::unique_ptr<Scanner> scanner,
             std::unique_ptr<mock_printer::MockPrinter> mock_printer,
             const base::FilePath& document_output_path,
             const base::FilePath& http_output_dir);

  // Receives the usb data in `data` which is http, process them as per
  // `usb_request` and returns the result in optional `SmartBuffer`. If return
  // value doesn't contain any value then it means that operation was stalled.
  std::optional<SmartBuffer> HandleUsbData(const Urb& usb_request,
                                           const SmartBuffer& data) override;

 private:
  // Below functions are used for IPP requests.
  // Receives IPP data and responds with the data received acknowledgment.
  void HandleIppUsbData(IConnection* conn, const Urb& usb_request);
  // Parses http header then aggregates all data chunks to the endpoint
  // interface and generates http response.
  void HandleHttpData(const Urb& usb_request, SmartBuffer* message);
  // Queues http responses to the endpoint interface.
  // Responses are sent back to the user when requested via bulk request.
  void QueueHttpResponse(const Urb& usb_request, const HttpResponse& response);
  // If desired (by a non-empty http_output_dir_), this will create a file and
  // log the HTTP info to this file.
  void LogHttpHeaders(const HttpRequest& request);
  // If desired (by a non-empty http_output_dir_), this will create a file and
  // log the IPP info to this file.
  void LogIppHeaders(const IppHeader& ipp_header);

  // Generates http responses for the printing requests identified by
  // `request`.
  HttpResponse GenerateHttpResponse(const HttpRequest& request,
                                    SmartBuffer* body);

  // Generates http response as per device implementation.
  HttpResponse GenerateHttpResponseForRequest(const HttpRequest& request,
                                              SmartBuffer* body);

  // Uses `IppManager` to handle the printing operation and returns http
  // response.
  HttpResponse GenerateHttpResponseFromIppManager(SmartBuffer* body);

  std::unique_ptr<IppManager> ipp_manager_;
  std::unique_ptr<Scanner> scanner_;
  std::unique_ptr<mock_printer::MockPrinter> mock_printer_;

  base::FilePath document_output_path_;
  base::FilePath http_output_dir_;

  // Counter used for logging both HTTP & IPP protocol messages
  int http_ipp_log_counter_;
};

#endif  // VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_USB_PRINTER_H_
