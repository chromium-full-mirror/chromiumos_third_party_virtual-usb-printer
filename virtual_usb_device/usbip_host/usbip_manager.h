// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_MANAGER_H_
#define VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_MANAGER_H_

#include <memory>
#include <vector>

#include <base/containers/flat_map.h>

#include "usbip_server.h"
#include "device_proxy.h"
#include "virtual-usb-printer/virtual_usb_device/usbip.h"

// This class implements the server side of USB/IP protocol. It receives usbip
// messages from the usbip client, filters actual usb requests and sends to the
// usb device (via device proxy). It also replies back to the client with usbip
// response messages.
class UsbIpManager {
 public:
  UsbIpManager();
  ~UsbIpManager();

  UsbIpManager(const UsbIpManager&) = delete;
  UsbIpManager& operator=(const UsbIpManager&) = delete;

  // Start the server which listens on port.
  void Run();
  // Stop the server.
  void Stop() { server_->Stop(); }

  // Register usb device with the UsbIpManager. This creates a proxy of the
  // actual usb device, which communicates with the usb device via socket. An
  // usbip client can list (e.g `usbip list` command) or interact with the
  // registered usb devices only.
  void RegisterDevice(const uint32_t port,
                      const uint16_t device_type,
                      const uint32_t device_id);
  void DeregisterDevice(const uint32_t port);

  // Handles OP_REQ_DEVLIST command.
  // Collect all registered(exported) usb devices and response
  // back to the user.
  bool HandleDeviceList(const base::ScopedFD& connection);

  // Handles OP_REQ_IMPORT command.
  // Internally, It maintains mapping of socket<-->device proxy. Any messages
  // received on socket is fowarded to mapped device proxy.
  void HandleAttach(const base::ScopedFD& connection);

  // Handle usbip messages with usb requests after devices are attached.
  // Requests could be of folowing type: control, bulk, ipp, raw usb data.
  bool HandleUsbRequest(const base::ScopedFD& connection,
                        bool* connection_open);

  // Process the usbip messages as per type of message.
  void HandleConnection(base::ScopedFD connection);

  // Handles OP_REQ_BIND_CMD usbip message.
  // OP_REQ_BIND_CMD is used to register usb device with UsbipManager.
  // Note: OP_REQ_BIND_CMD is custom defined.
  void HandleBind(const base::ScopedFD& connection);

  // Handles following OP usbip messages:
  // OP_REQ_BIND_CMD : Custom defined usbip message used for
  // registration of device.
  // OP_REQ_DEVLIST_CMD : Lists exported device list.
  // OP_REQ_IMPORT_CMD : Import usb device for further communication.
  // More details about these messages are documented in
  // https://docs.kernel.org/usb/usbip_protocol.html.
  bool HandleOpRequest(const base::ScopedFD& connection,
                       bool* attached,
                       bool* connection_open);

  // Handles USBIP_CMD_SUBMIT messages after device is imported.
  void HandleUsbCommand(int sockfd, const UsbipCmdSubmit& usb_request);

  // Count of devices registered.
  size_t NoOfExportedDevice() { return clients.size(); }

 private:
  // Reads port number from connection and return DeviceProxy index
  // associated with port.
  int ReadPort(const base::ScopedFD& sockfd);

  // Determines whether `usb_request` is either a standard or class-specific
  // control request and defers to the corresponding function.
  void HandleUsbControl(int sockfd, const UsbipCmdSubmit& usb_request);

  std::unique_ptr<Server> server_;

  // List of clients for registered usb devices.
  std::vector<std::unique_ptr<DeviceProxy>> clients;
  // mapping of socket<-->DeviceProxy Index in clients.
  base::flat_map<int, int> attachments_;
};

#endif  // VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_MANAGER_H_
