// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_CORE_H_
#define VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_CORE_H_

#include <memory>
#include <vector>

#include <base/memory/scoped_refptr.h>

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
  bool RegisterDevice(const uint16_t port, const OpRepDevInfo& device_info);
  // Deregisters the usb device from `UsbIpCore`.
  void DeregisterDevice(const uint16_t port);

  // Disconnects with the attached usb device and is triggered by `usbip detach`
  // command.
  void DetachUsbDevice(IConnection* conn);

  // Handle OP_REQ_BIND_CMD usbip command by registering the usb device with
  // `UsbIpCore`.
  // Note: OP_REQ_BIND_CMD is custom defined usbip message.
  bool HandleBindInternal(IConnection* conn, uint16_t port);

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
  void HandleUsbCommandSubmit(IConnection* conn,
                              const UsbipCmdSubmit& usb_request);

 private:
  // Reads busid from `sockfd` and returns DeviceProxy index
  // associated with the busid.
  int ReadBusId(IConnection* conn);

  // Updates the busID, usbPath and devnum, so that for each new device it is
  // unique.
  void UpdateDeviceParameter(OpRepDevice* device);

  // Sends usbip response with acknowledgement of `received` amount of data
  // received by usb device.
  void SendUsbipDataReceivedAck(IConnection* conn,
                                const UsbipCmdSubmit& usb_request,
                                size_t received);

  //  Sends back a usbip response after `usb_request` is handled. The `data` are
  //  response data with size `data_size`.
  void SendUsbipControlResponse(IConnection* conn,
                                const UsbipCmdSubmit& usb_request,
                                const uint8_t* data,
                                size_t data_size);

  // List of proxy for each registered usb device.
  std::vector<std::unique_ptr<DeviceProxy>> proxy_list;

  // Number of usb devices registered with `UsbIpCore`
  size_t device_count = 0;
};

#endif  // VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_CORE_H_
