// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USBIP_HOST_DEVICE_PROXY_H_
#define VIRTUAL_USB_DEVICE_USBIP_HOST_DEVICE_PROXY_H_

#include <cstdint>
#include <memory>
#include <string>

#include <base/functional/callback.h>

#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/virtual_usb_device/connection/connection.h"
#include "virtual-usb-printer/virtual_usb_device/op_commands.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"

// This class acts as a proxy for actual usb device, similar to dbus proxy
// classes. It communicates with usb device process and receives responses of
// the usb operations. There is single `DeviceProxy` for each usb device
// registered with the usbip host.
class DeviceProxy {
 public:
  using CreateCb = base::RepeatingCallback<std::unique_ptr<DeviceProxy>(
      const uint16_t, const ::BusId&, const size_t)>;

  static std::unique_ptr<DeviceProxy> Create(const uint16_t port,
                                             const BusId& id,
                                             const size_t devnum);
  // Create a proxy object for usb device which listens on
  // `port` with usb information `info`.
  DeviceProxy(const uint16_t port, const ::BusId& id, const size_t devnum);
  virtual ~DeviceProxy();

  // Connect with usb devices process via socket.
  // Return true on successful connection, otherwise returns false.
  virtual bool ConnectDevice();
  // Close socket connection to usb devices.
  virtual void DisconnectDevice();

  // Connects with the device server and attaches a connection `conn` to this
  // proxy. All requests received on the `conn` will be handled by this
  // proxy.
  void Attach(const IConnection* conn);

  // Disconnect with the device server and removes the attached connection.
  void Detach();

  // Returns the attached socket descriptor.
  int GetAttachedSocket() const { return attached_socket_; }

  // Handles usb request by sending the request and data to the usb
  // device running in different process and receiving the result back in
  // `response_data`.
  // `stalled` is set to true if usb request is stalled in the device process
  // The output params are populated as:
  // if `stalled == true` then command is stalled and thus `response_data` must
  // be empty.
  // if `stalled == false` then command is processed. `response_data` contains
  // the result of operation which may be empty.
  // if `response_data` is non-empty then `stalled` must be `false`.
  // All of above cases denotes success and thus returns `true`. `false`
  // is returned when connection to the device process fails.
  virtual bool HandleUsbDeviceRequest(const Urb& urb_request,
                                      const SmartBuffer& data,
                                      SmartBuffer* response_data,
                                      bool* stalled);
  // Returns port number to which this device proxy is connected to.
  uint16_t GetPort() const { return device_port_; }

  // Returns bus id of device.
  std::string BusId() const { return std::string(device_info_.device.busID); }

  // Returns whether this proxy is connected with actual usb device.
  bool IsConnected() const { return is_connected_; }
  bool IsAttached() const { return (attached_socket_ != -1 ? true : false); }
  // Sends usb requests to the usb device to fetch device descriptors, based on
  // which `OpRepDevInfo` structure is prepared.
  // Returns false, if there are any error in sending the usb requests.
  bool GetOpRepDevInfo(OpRepDevInfo* info);

 private:
  // Receives `size` amount of response data. The size is determined by the
  // `HandleUsbDeviceRequest` call.
  // Returns false if there are any communication error with device.
  bool ReadUsbResponseData(const size_t size, SmartBuffer* response_data);

  // connection to device server
  std::unique_ptr<IConnection> connection_;

  bool is_connected_ = false;

  OpRepDevInfo device_info_;

  int attached_socket_ = -1;

  const uint16_t device_port_;
  const ::BusId busid_;
  const size_t devnum_;
};
#endif  // VIRTUAL_USB_DEVICE_USBIP_HOST_DEVICE_PROXY_H_
