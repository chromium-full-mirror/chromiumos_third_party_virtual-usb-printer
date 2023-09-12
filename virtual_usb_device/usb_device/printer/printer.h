// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_PRINTER_H_
#define VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_PRINTER_H_

#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/containers/flat_map.h>

#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/http_util.h"
#include "virtual-usb-printer/virtual_usb_device/smart_buffer.h"
#include "virtual-usb-printer/virtual_usb_device/usbip.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/virtual_usb_device/device_request.h"
#include "virtual-usb-printer/virtual_usb_device/server.h"

// This class is responsible for managing an ippusb interface of a printer. It
// keeps track of whether or not the interface is currently in the process of
// receiving a chunked IPP message, and queues up responses to IPP requests so
// they can be sent when a BULK IN request is received.
class InterfaceManager {
 public:
  InterfaceManager() = default;

  // Place the IPP response `message` on the end of `queue_`.
  void QueueMessage(const SmartBuffer& message);

  // Returns whether or not `queue_` is empty.
  bool QueueEmpty() const;

  // Returns the message at the front of `queue_` and removes it. If PopMessage
  // is called when `queue_` is empty then the program will exit.
  SmartBuffer PopMessage();

  // Having just received some HTTP data, returns the message if
  // it is complete; returns nullopt otherwise.
  std::optional<SmartBuffer> AddMessageAndReturnIfComplete(
      const SmartBuffer& message);

  bool receiving_message() const { return receiving_message_; }
  void set_receiving_message(bool b) { receiving_message_ = b; }

  bool receiving_chunked() const { return receiving_chunked_; }
  void set_receiving_chunked(bool b) { receiving_chunked_ = b; }

  HttpRequest request_header() const { return request_header_; }
  void set_request_header(const HttpRequest& r) { request_header_ = r; }

  SmartBuffer* message() { return &message_; }

 private:
  std::queue<SmartBuffer> queue_;
  // Represents whether the interface is currently receiving an HTTP message.
  bool receiving_message_;
  // Represents whether the interface is currently receiving an HTTP "chunked"
  // message.
  bool receiving_chunked_;
  HttpRequest request_header_;
  SmartBuffer message_;
};

// A grouping of the descriptors for a USB device.
class UsbDescriptors {
 public:
  // Create a new UsbDescriptors object using the USB descriptors defined in
  // JSON format in the file located at `descriptor_file`.
  static std::optional<UsbDescriptors> Create(
      const std::string& descriptor_file);

  const UsbDeviceDescriptor& device_descriptor() const {
    return device_descriptor_;
  }

  const UsbConfigurationDescriptor& configuration_descriptor() const {
    return configuration_descriptor_;
  }

  const UsbDeviceQualifierDescriptor& qualifier_descriptor() const {
    return qualifier_descriptor_;
  }

  const std::vector<std::vector<char>>& string_descriptors() const {
    return string_descriptors_;
  }

  const std::vector<char>& ieee_device_id() const { return ieee_device_id_; }

  const std::vector<UsbInterfaceDescriptor>& interface_descriptors() const {
    return interface_descriptors_;
  }

  const base::flat_map<uint8_t, std::vector<UsbEndpointDescriptor>>&
  endpoint_descriptors() const {
    return endpoint_descriptors_;
  }

 private:
  explicit UsbDescriptors(
      const UsbDeviceDescriptor& device_descriptor,
      const UsbConfigurationDescriptor& configuration_descriptor,
      const UsbDeviceQualifierDescriptor& qualifier_descriptor,
      const std::vector<std::vector<char>>& string_descriptors,
      const std::vector<char>& ieee_device_id,
      const std::vector<UsbInterfaceDescriptor>& interfaces,
      const base::flat_map<uint8_t, std::vector<UsbEndpointDescriptor>>&
          endpoints);

  UsbDeviceDescriptor device_descriptor_;
  UsbConfigurationDescriptor configuration_descriptor_;
  UsbDeviceQualifierDescriptor qualifier_descriptor_;

  // Represents the strings attributes of the printer.
  // These strings may contain '\0' character.
  // For more information about strings descriptors refer to: Section 9.6
  // Standard USB Descriptor Definitions
  // https://www.usb.org/document-library/usb-20-specification-released-april-27-2000
  std::vector<std::vector<char>> string_descriptors_;

  // IEEE device id specified by usb descriptor. It may contain a '\0' byte.
  std::vector<char> ieee_device_id_;
  std::vector<UsbInterfaceDescriptor> interface_descriptors_;

  // Maps interface numbers to their respective collection of endpoint
  // descriptors.
  base::flat_map<uint8_t, std::vector<UsbEndpointDescriptor>>
      endpoint_descriptors_;
};

// Represents a single USB printer.
// It processes control and printer-specific usb requests and respond back
// with result.
// Each usb device register itself with UsbIpHost and listen for usb requests.
// TODO(santo) : Move usb specific stuffs in new class called `UsbDevice` and
// keep only printer related code in this class.
// The generic `UsbDevice` class should be used as base class to
// define any new usb devices.
class Printer {
 public:
  // This parses command line parameters as described in `kUsage` variable
  // (see kUsage definition in printer.cc) and create Printer device object. If
  // paramaters are incorrect then process terminate after printing kUsage
  // content.
  static Printer Create(int argc, char* argv[]);

  // Starts the tcp server and listen for usb commands.
  void Run();

  const UsbDeviceDescriptor& device_descriptor() const {
    return usb_descriptors_.device_descriptor();
  }

  const UsbConfigurationDescriptor& configuration_descriptor() const {
    return usb_descriptors_.configuration_descriptor();
  }

  const UsbDeviceQualifierDescriptor& qualifier_descriptor() const {
    return usb_descriptors_.qualifier_descriptor();
  }

  const std::vector<std::vector<char>>& string_descriptors() const {
    return usb_descriptors_.string_descriptors();
  }

  const std::vector<char>& ieee_device_id() const {
    return usb_descriptors_.ieee_device_id();
  }

  const std::vector<UsbInterfaceDescriptor>& interface_descriptors() const {
    return usb_descriptors_.interface_descriptors();
  }

  const base::flat_map<uint8_t, std::vector<UsbEndpointDescriptor>>&
  endpoint_descriptors() const {
    return usb_descriptors_.endpoint_descriptors();
  }

  // Handles specific usb command requests.
  // All types of usb requests are defined in device_request.h.
  void HandleRequest(int sockfd, const uint32_t request);

  // Determines whether `usb_request` is either a control or data request, and
  // handle accordingly.
  void HandleUsbRequest(int sockfd, const UsbipCmdSubmit& usb_request);

 private:
  // Create printer object from usb descriptors.
  // UsbDescriptors are created by reading JSON config file.
  // `document_output_path` is a path where final document
  // is outputted (simulation of printing).
  Printer(const UsbDescriptors& usb_descriptors,
          const base::FilePath& document_output_path);

  // Determines whether `usb_request` is either a standard or class-specific
  // control request and defers to the corresponding function.
  void HandleUsbControl(int sockfd, const UsbipCmdSubmit& usb_request) const;

  // Acknowledge the request and appends data to file.
  void HandleUsbData(int sockfd, const UsbipCmdSubmit& usb_request) const;

  // Handles usb request of type=STANDARD_TYPE.
  void HandleStandardControl(int sockfd,
                             const UsbipCmdSubmit& usb_request,
                             const UsbControlRequest& control_request) const;

  // Handler for following Standard control request:
  // GET_STATUS, GET_DESCRIPTOR, GET_CONFIGURATION.
  void HandleGetStatus(int sockfd,
                       const UsbipCmdSubmit& usb_request,
                       const UsbControlRequest& control_request) const;
  void HandleGetDescriptor(int sockfd,
                           const UsbipCmdSubmit& usb_request,
                           const UsbControlRequest& control_request) const;
  void HandleGetConfiguration(int sockfd,
                              const UsbipCmdSubmit& usb_request,
                              const UsbControlRequest& control_request) const;

  // The below functions responds with usb descriptors of the devices.
  // The descriptors category are: device descriptor, configuration descriptor,
  // string descriptor and qualifier descriptor.
  void HandleGetDeviceDescriptor(
      int sockfd,
      const UsbipCmdSubmit& usb_request,
      const UsbControlRequest& control_request) const;
  void HandleGetConfigurationDescriptor(
      int sockfd,
      const UsbipCmdSubmit& usb_request,
      const UsbControlRequest& control_request) const;
  void HandleGetDeviceQualifierDescriptor(
      int sockfd,
      const UsbipCmdSubmit& usb_request,
      const UsbControlRequest& control_request) const;
  void HandleGetStringDescriptor(
      int sockfd,
      const UsbipCmdSubmit& usb_request,
      const UsbControlRequest& control_request) const;

  // Handles printer control specific USB requests.
  void HandlePrinterControl(int sockfd,
                            const UsbipCmdSubmit& usb_request,
                            const UsbControlRequest& control_request) const;

  // Respond with ieee device id of the printer.
  void HandleGetDeviceId(int sockfd,
                         const UsbipCmdSubmit& usb_request,
                         const UsbControlRequest& control_request) const;

  // Used to send an empty response to control requests which are not supported.
  void HandleUnsupportedRequest(int sockfd,
                                const UsbipCmdSubmit& usb_request,
                                const UsbControlRequest& control_request) const;

  // Responds to a BULK_IN request by replying with the message at the front of
  // `message_queue_`.
  void HandleBulkInRequest(int sockfd, const UsbipCmdSubmit& usb_request);

  UsbDescriptors usb_descriptors_;
  base::FilePath document_output_path_;
  base::FilePath http_output_dir_;

  std::vector<InterfaceManager> interface_managers_;
  std::unique_ptr<Server> server_;
};

#endif  // VIRTUAL_USB_DEVICE_USB_DEVICE_PRINTER_PRINTER_H_
