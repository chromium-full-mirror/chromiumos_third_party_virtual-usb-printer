// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USB_DEVICE_USB_DEVICE_H_
#define VIRTUAL_USB_DEVICE_USB_DEVICE_USB_DEVICE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/containers/flat_map.h>

#include "virtual-usb-printer/common/http_util.h"
#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/server.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"

// This class is responsible for managing an ippusb interface of a printer. It
// keeps track of whether or not the interface is currently in the process of
// receiving a chunked IPP message, and queues up responses to IPP requests so
// that they can be sent when a BULK IN request is received.
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
  // it is complete; returns `nullopt` otherwise.
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
  UsbDescriptors() = default;
  // Create a new UsbDescriptors object using the USB descriptors defined in
  // JSON format in the file located at `descriptor_file`.
  static std::optional<UsbDescriptors> Create(
      const std::string& descriptor_file);
  static std::optional<UsbDescriptors> CreateFromJson(
      const std::string& descriptors_contents);

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

  // Represents the strings attributes of the usb device.
  // Since these strings may contain '0' bytes, using std::string to
  // represent them isn't safer when used/passed as char* in any part of code.
  // since that would be interpreted as end-of-string
  // For more information about strings descriptors refer to: Section 9.6
  // Standard USB Descriptor Definitions
  // https://www.usb.org/document-library/usb-20-specification-released-april-27-2000
  std::vector<std::vector<char>> string_descriptors_;

  // As with USB string descriptors the IEEE device id may contain a 0 byte in
  // the portion which indicates the message size, so a vector is used instead
  // of a string.
  std::vector<char> ieee_device_id_;
  std::vector<UsbInterfaceDescriptor> interface_descriptors_;

  // Maps interface numbers to their respective collection of endpoint
  // descriptors.
  base::flat_map<uint8_t, std::vector<UsbEndpointDescriptor>>
      endpoint_descriptors_;
};

// Represents a single USB device that can respond to basic USB control
// and device-specific USB requests.
// Each usb device registers itself with the usbip host, listens for the usb
// commands on fixed port, and responds back result to the usbip host.
class UsbDevice : public ConnectionHandler {
 public:
  // Create generic usb device object which listens for usb requests on
  // `port`. The usb descriptors for the device are provided by `descriptors`.
  explicit UsbDevice(const UsbDescriptors& descriptors);
  virtual ~UsbDevice() = default;

  // Starts the tcp server on `port` and listen for usb commands.
  // It also registers this device with the usbip host running on `host_port`.
  void Run(const uint16_t port, const uint16_t host_port);

  // Returns different usb descriptors.
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

  // Returns `InterfaceManager` for specified endpoint.
  InterfaceManager* GetInterfaceManager(int endpoint);

  void HandleConnection(std::unique_ptr<IConnection> conn) override;

  // Processes `usb_request` with `data` and returns the result of usb operation
  // in optional `SmartBuffer`. If returned value doesn't contain a value then
  // it means the operation was STALLED.
  // Note: Return value can have an empty `SmartBuffer` and it means that
  // operation was processed, although result was empty.
  std::optional<SmartBuffer> HandleUsbRequest(const Urb& usb_request,
                                              const SmartBuffer& data);

 private:
  // Processes usb control requests and returns the result in optional
  // `SmartBuffer`. If optional return value doesn't contain a value then it
  // means that the operation was `STALLED`.
  std::optional<SmartBuffer> HandleUsbControl(const Urb& usb_request) const;

  // Processes Raw or IPP usb data and returns the result in optional
  // `SmartBuffer`. If optional return value doesn't contain a value then it
  // means that the operation was `STALLED`.
  virtual std::optional<SmartBuffer> HandleUsbData(const Urb& usb_request,
                                                   const SmartBuffer& data) {
    return std::nullopt;
  }

  // Handles usb requests of type=STANDARD_TYPE.
  SmartBuffer HandleStandardControl(
      const Urb& usb_request, const UsbControlRequest& control_request) const;

  // Standard control request handlers for GET_STATUS, GET_DESCRIPTOR,
  // GET_CONFIGURATION requests.
  virtual SmartBuffer HandleGetStatus(
      const Urb& usb_request, const UsbControlRequest& control_request) const;
  SmartBuffer HandleGetDescriptor(
      const Urb& usb_request, const UsbControlRequest& control_request) const;
  SmartBuffer HandleGetConfiguration(
      const Urb& usb_request, const UsbControlRequest& control_request) const;

  // Below functions respond with the descriptors of usb device.
  // The descriptors category are: device descriptors, configuration
  // descriptors, string descriptors, device qualifier descriptors.
  SmartBuffer HandleGetDeviceDescriptor(
      const Urb& usb_request, const UsbControlRequest& control_request) const;
  SmartBuffer HandleGetConfigurationDescriptor(
      const Urb& usb_request, const UsbControlRequest& control_request) const;
  SmartBuffer HandleGetDeviceQualifierDescriptor(
      const Urb& usb_request, const UsbControlRequest& control_request) const;
  SmartBuffer HandleGetStringDescriptor(
      const Urb& usb_request, const UsbControlRequest& control_request) const;

  // Handles usb device control specific USB requests and returns the result in
  // optional `SmartBuffer`. If return value doesn't contain a value it means
  // that operation was STALLED.
  std::optional<SmartBuffer> HandleDeviceControl(
      const Urb& usb_request, const UsbControlRequest& control_request) const;

  // Responds with the ieee device id of usb device.
  SmartBuffer HandleGetDeviceId(const Urb& usb_request,
                                const UsbControlRequest& control_request) const;

  // Used to send an empty response to control requests which are not supported.
  SmartBuffer HandleUnsupportedRequest(
      const Urb& usb_request, const UsbControlRequest& control_request) const;

  // Responds to a BULK_IN request by replying with the message at the front of
  // `message_queue_`.
  SmartBuffer HandleBulkInRequest(const Urb& usb_request);

  void SendUsbResponse(IConnection* conn,
                       const Urb& usb_request,
                       const uint8_t* data,
                       size_t data_size,
                       bool is_stalled) const;

  UsbDescriptors usb_descriptors_;
  base::FilePath document_output_path_;
  base::FilePath http_output_dir_;

  std::vector<InterfaceManager> interface_managers_;

  std::unique_ptr<Server> device_server_;
};

#endif  // VIRTUAL_USB_DEVICE_USB_DEVICE_USB_DEVICE_H_
