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

// This class deals with usbip messages recevied from the user.
// It receives usbip messages, filter the usb command and forward them
// to the registered usb device (which runs on seperate process).
// It also maintains the list of proxy for each registered usb device
// which is used to communicate with actual device process.
class UsbIpManager {
 public:
  UsbIpManager();
  ~UsbIpManager();

  UsbIpManager(const UsbIpManager&) = delete;
  UsbIpManager& operator=(const UsbIpManager&) = delete;

  // Start the server which listens on port.
  void Run();
  // Stops the server.
  void Stop() { server_->Stop(); }

  // Loops and process usbip message received on connection.
  void HandleConnection(base::ScopedFD connection);

  // Process OP_REQ_BIND_CMD usbip message by registering the usb device with
  // `UsbIpManager`.
  // Note: OP_REQ_BIND_CMD is custom defined usbip message.
  void HandleBind(const base::ScopedFD& connection);

  // Register the usb device with `UsbIpManager`.
  // Once this is called, a proxy of actual usb device is created and
  // only after registeration, device can be attached by the user
  void RegisterDevice(const uint16_t port, const OpRepDevInfo& device_info);
  // TODO(santo): Figure out when to deregister.
  void DeregisterDevice(const uint16_t port);

  // Handles OP_REQ_DEVLIST command by responding with the list of exported usb
  // devices.
  bool HandleDeviceList(const base::ScopedFD& connection);
  // Handles OP_REQ_IMPORT command by attaching a usb device for further
  // communication. Internally, a `DeviceProxy` is used to make tcp connection
  // to the usb device server. Any subsequent usb messages on that proxy will be
  // forwarded to attached usb device.
  bool HandleAttach(const base::ScopedFD& connection);

  // Handle usb requests after usb device is attached.
  // Requests could be of following type: control, bulk, ipp, raw usb data.
  bool HandleUsbRequest(const base::ScopedFD& connection,
                        bool* connection_open);

  // Handle Op usbip requests, which is of following type
  // OP_REQ_BIND_CMD : custom defined for registeration
  // OP_REQ_DEVLIST_CMD : Lists exported device list
  // OP_REQ_IMPORT_CMD : Attach requested device to
  // receive usb command directly
  bool HandleOpRequest(const base::ScopedFD& connection, bool* attached);

 private:
  // Respond with status of OP_REQ_BIND_CMD usbip message.
  void RespondBind(const base::ScopedFD& connection, bool status);
  // Read port number from connection and return DeviceProxy index
  // associated with port.
  int ReadBusId(const base::ScopedFD& sockfd);

  // Handles usbip messages after device is attached.
  // It retrieves the usb command and forward to UsbIpManager
  void HandleUsbCommand(int sockfd, const UsbipCmdSubmit& usb_request);
  // Update busID, usbPath and devnum, so that for each new device it is unique.
  void UpdateDeviceParameter(OpRepDevice* device);

  std::unique_ptr<Server> server_;

  // List of proxy for each registered usb device.
  std::vector<std::unique_ptr<DeviceProxy>> proxy_list;

  size_t device_count = 0;
};

#endif  // VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_MANAGER_H_
