// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_MANAGER_H_
#define VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_MANAGER_H_

#include <memory>
#include <vector>

#include <base/containers/flat_map.h>

#include "device_proxy.h"
#include "usbip_server.h"
#include "virtual-usb-printer/virtual_usb_device/usbip.h"

// This class deals with usbip messages recevied from the user.
// It receives usbip messages, filter the usb command and forward them
// to the registered usb device (which runs on seperate process).
// It also maintains the list of proxy for each registered usb device
// which is used to communicate with actual device process.
class UsbIpManager {
 public:
  // Creates object which listens on `port` for usbip messages.
  explicit UsbIpManager(uint16_t port);
  ~UsbIpManager();

  UsbIpManager(const UsbIpManager&) = delete;
  UsbIpManager& operator=(const UsbIpManager&) = delete;

  // Starts the server which listens on port.
  void Run();
  // Stops the server.
  void Stop() { server_->Stop(); }

  // Loops and process usbip message received on connection.
  void HandleConnection(std::unique_ptr<IConnection> conn);

  // Process OP_REQ_BIND_CMD usbip message by registering the usb device with
  // `UsbIpManager`.
  // Note: OP_REQ_BIND_CMD is custom defined usbip message.
  void HandleBind(IConnection* conn);

  // Registers the usb device with `UsbIpManager`.
  // Once this is called, a proxy of actual usb device is created and
  // only after registeration, device can be attached by the user
  void RegisterDevice(const uint16_t port, const OpRepDevInfo& device_info);
  // TODO(santo): Figure out when to deregister.
  void DeregisterDevice(const uint16_t port);
  // Disconnects with the attached usb device and is triggered by `usbip detach`
  // command.
  void DetachUsbDevice(IConnection* conn);

  // Handles OP_REQ_DEVLIST command by responding with the list of exported usb
  // devices.
  void HandleDeviceList(IConnection* conn);
  // Handles OP_REQ_IMPORT command by attaching a usb device for further
  // communication. Internally, a `DeviceProxy` is used to make tcp connection
  // to the usb device server. Any subsequent usb messages on that proxy will be
  // forwarded to attached usb device.
  // Returns whether usb device is successfully attached or not.
  bool HandleAttach(IConnection* conn);

  // Handles usb requests after usb device is attached.
  // Requests could be of following type: control, bulk, ipp, raw usb data.
  // A true return value means conn to the client should
  // be kept open after servicing the request.
  bool HandleUsbRequest(IConnection* conn);

  // Handles following usbip OP requests.
  // OP_REQ_BIND_CMD : custom defined for registeration.
  // OP_REQ_DEVLIST_CMD : Lists exported device list.
  // OP_REQ_IMPORT_CMD : Attach requested device to
  // receive usb command directly.
  // A true return value means connection to the client should
  // be kept open after servicing the request.
  bool HandleOpRequest(IConnection* conn, bool* attached);

 private:
  // Responds with the status of OP_REQ_BIND_CMD usbip message.
  void RespondBind(IConnection* conn, bool status);
  // Reads busid from `sockfd` and returns DeviceProxy index
  // associated with the busid.
  int ReadBusId(IConnection* conn);

  // Handles usbip requests after the device is attached.
  // It retrieves the usb command and forward it to the `UsbIpManager`.
  void HandleUsbCommand(IConnection* conn, const UsbipCmdSubmit& usb_request);

  //  Sends a usbip response on socket `sockfd`. The `data` are
  //  response data with size `data_size`.
  void SendUsbipControlResponse(IConnection* conn,
                                const UsbipCmdSubmit& usb_request,
                                const uint8_t* data,
                                size_t data_size);
  // Updates the busID, usbPath and devnum, so that for each new device it is
  // unique.
  void UpdateDeviceParameter(OpRepDevice* device);

  std::unique_ptr<Server> server_;

  // List of proxy for each registered usb device.
  std::vector<std::unique_ptr<DeviceProxy>> proxy_list;

  size_t device_count = 0;
};

#endif  // VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_MANAGER_H_
