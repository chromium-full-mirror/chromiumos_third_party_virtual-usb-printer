// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_CORE_H_
#define VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_CORE_H_

#include <memory>
#include <vector>
#include <string>

#include <base/containers/flat_map.h>

#include "device_proxy.h"

// This class handles usbip requests recevied from the user.  It filters the raw
// usb command from usbip requests and forward them to the registered usb device
// (which runs on seperate process). It also maintains the list of proxy for
// each registered usb device which is used to communicate with actual device
// process. This class is not thread safe, thus to avoid data races, all the
// functions of this class must be run in its own task runner sequence.
class UsbIpCore {
 public:
  // Creates object with internal task runner.
  UsbIpCore() = default;
  ~UsbIpCore() = default;

  UsbIpCore(const UsbIpCore&) = delete;
  UsbIpCore& operator=(const UsbIpCore&) = delete;

  // Registers the usb device with `UsbIpCore`.
  // Once this is called, a proxy of actual usb device is created and only after
  // registeration, device can be listed or attached by the user.
  // Device is deregistered whenever device process is inaccessible during
  // operations.
  bool RegisterDevice(const uint16_t port);

  // Disconnects with the attached usb device and is triggered by `usbip detach`
  // command.
  void DetachUsbDevice(IConnection* conn);

  // Handles OP_REQ_DEVLIST command by responding with the list of exported usb
  // devices.
  void HandleDeviceListInternal(IConnection* conn);

  // Handles OP_REQ_IMPORT command by attaching a usb device for further
  // communication. Internally, a `DeviceProxy` is used to make tcp connection
  // to the usb device server. Any subsequent usb messages on that proxy will be
  // forwarded to attached usb device.
  // Returns whether usb device is successfully attached or not.
  bool HandleAttachInternal(IConnection* conn);

  // Handles usbip submit command after the device is attached.
  // It retrieves the raw usb command, forwards it to the usb device
  // and wait for result of usb operation which is responded back.
  bool HandleUsbCommandSubmit(IConnection* conn,
                              const UsbipCmdSubmit& usb_request);

 private:
  // Reads busid from connection and returns it.
  BusId ReadBusId(IConnection* conn);

  // Handles usb request with given `DeviceProxy`. Returns `false` on error
  // or `true` on success
  bool HandleUsbCommandSubmitWithProxy(IConnection* conn,
                                       const UsbipCmdSubmit& usb_request,
                                       std::unique_ptr<DeviceProxy>& proxy);

  // Sends usbip response having ack of `received` amount of data received
  // by usb device.
  void SendUsbipDataReceivedAck(IConnection* conn,
                                const UsbipCmdSubmit& usb_request,
                                size_t received);

  //  Sends back a usbip response after `usb_request` is handled. The `data` are
  //  response data with size `data_size`.
  void SendUsbipControlResponse(IConnection* conn,
                                const UsbipCmdSubmit& usb_request,
                                const uint8_t* data,
                                size_t data_size);

  // map of busid<--> device proxy
  base::flat_map<BusId, std::unique_ptr<DeviceProxy>> proxy_map_;

  size_t device_count_ = 0;
};

#endif  // VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_CORE_H_
