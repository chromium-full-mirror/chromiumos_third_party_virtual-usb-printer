// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_OP_COMMANDS_H_
#define VIRTUAL_USB_DEVICE_OP_COMMANDS_H_

/*
 * This file defines the supported OP command messages from the usbip userspace
 * protocol, as well as some utility functions for processing them.
 *
 * In the context of the defined messages:
 *   "Req" is used in messages that submit a request.
 *   "Rep" is used in messages which reply to a request.
 *
 * For more information about the usbip protocol refer to the following
 * documentation:
 * https://www.kernel.org/doc/Documentation/usb/usbip_protocol.txt
 * https://en.opensuse.org/SDB:USBIP
 */

#include <cstdint>
#include <array>
#include <vector>

#include "device_descriptors.h"
#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "usbip_constants.h"

constexpr uint16_t kUsbipVersion = 0x0111;  // usbip version in BCD.

using BusId = std::array<char, 32>;

// Contains the header values that are contained within all of the "OP" messages
// used by usbip.
struct OpHeader {
  uint16_t version;  // usbip version
  uint16_t command;  // op command type
  int status;        // op request status
};

// Generic device descriptor used by OpRepDevlist and OpRepImport.
struct OpRepDevice {
  char usbPath[256];
  char busID[32];
  uint32_t busnum;
  uint32_t devnum;
  uint32_t speed;
  uint16_t idVendor;
  uint16_t idProduct;
  uint16_t bcdDevice;
  uint8_t bDeviceClass;
  uint8_t bDeviceSubClass;
  uint8_t bDeviceProtocol;
  uint8_t bConfigurationValue;
  uint8_t bNumConfigurations;
  uint8_t bNumInterfaces;
};

// The OpReqDevlistMessage message contains the same information as OpHeader.
typedef OpHeader OpReqDevlist;

// The header used in an OpRepDevlist message, the only difference from
// OpHeader is that it contains `numExportedDevices`.
struct OpRepDevlistHeader {
  OpHeader header;
  int numExportedDevices;  // Number of registered usb device (i.e exported)
};

// Basic interface descriptor used by OpRepDevlist.
struct OpRepDeviceInterface {
  uint8_t bInterfaceClass;
  uint8_t bInterfaceSubClass;
  uint8_t bInterfaceProtocol;
  uint8_t padding;
};

// Single device information (descriptors+interfaces) used in
// OP_REP_DEVLIST response.
// OP_REQ_DEVLIST lists multiple usb device information and
// each device information is represented by this struct.
// (see struct OpRepDevlist also).
struct OpRepDevInfo {
  OpRepDevice device;
  std::vector<OpRepDeviceInterface> interfaces;
};

// Custom usbip message which is used to bind(register) or unbind(deregister)
// usb device with the host. When usb device is binded with the host, host saves
// the device information and the device is considered exported, i.e device can
// be listed or attached by usbip client.
// When usb device is Unbinded, host removes the device information and
// thus can't be further seen by the usbip client.
struct BindOrUnbindRequest {
  OpHeader header;
  uint16_t port;  // port on which usb device listens
};

// Defines the OpReqDevlist message.
struct OpRepDevlist {
  OpRepDevlistHeader header;
  std::vector<OpRepDevInfo> devices;
};

// Defines the OpReqImport request used to request a device for import.
struct OpReqImport {
  OpHeader header;
  char busID[32];
};

// Defines the OpRepImport response which indicates whether the requested device
// was successfully exported.
struct OpRepImport {
  OpHeader header;
  OpRepDevice device;
};

// Sets the corresponding members of `header` using the given values.
void SetOpHeader(uint16_t command, int status, OpHeader* header);

// Sets the corresponding members of `devlist_header` using the given values.
void SetOpRepDevlistHeader(uint16_t command,
                           int status,
                           int numExportedDevices,
                           OpRepDevlistHeader* header);

// Sets the members of `device` using the corresponding values in
// `dev_dsc` and `config`.
void SetOpRepDevice(const UsbDeviceDescriptor& dev_dsc,
                    const UsbConfigurationDescriptor& configuration,
                    OpRepDevice* device);

// Assigns the values from `interfaces` into `rep_interfaces`.
void SetOpRepDeviceInterfaces(
    const std::vector<UsbInterfaceDescriptor>& interfaces,
    std::vector<OpRepDeviceInterface>* rep_interfaces);

// Set header for OP_REP_DEVLIST message.
void CreateOpRepDevlistHeader(uint16_t numExportedDevices,
                              OpRepDevlist* message);

// Populate the OpRepDevInfo information for OP_REP_DEVLIST message,
// which is used to respond when a request to list all exported devices
// are received.
void CreateOpRepDevInfo(const UsbDeviceDescriptor& device,
                        const UsbConfigurationDescriptor& config,
                        const std::vector<UsbInterfaceDescriptor>& interfaces,
                        OpRepDevInfo* devinfo);

// Creates the OpRepImport message used to respond to a request to attach a
// host USB device.
void CreateOpRepImport(const OpRepDevInfo& devinfo, OpRepImport* rep);

// Convert the various elements of an "OpRep" message into network
// byte order and pack them into a SmartBuffer to be used for transferring along
// a socket.
SmartBuffer PackOpHeader(OpHeader header);
SmartBuffer PackOpRepDevice(OpRepDevice device);
SmartBuffer PackOpRepDevlistHeader(OpRepDevlistHeader devlist_header);
SmartBuffer PackOpRepDevlist(OpRepDevlist devlist);
SmartBuffer PackOpRepImport(OpRepImport message);
SmartBuffer PackOpReqImport(OpReqImport import);
SmartBuffer PackOpRepDevInfo(OpRepDevInfo& info);
SmartBuffer PackBindOrUnbindRequest(BindOrUnbindRequest& request);

OpHeader UnpackOpHeader(SmartBuffer* buf);
void UnpackOpHeader(OpHeader* header);
OpHeader UnpackOpHeader(SmartBuffer* buf);

void UnpackOpRepDevice(OpRepDevice* device);
OpRepDevice UnpackOpRepDevice(SmartBuffer* buf);
OpRepImport UnpackOpRepImport(SmartBuffer* buf);
void UnpackOpRepDevlistHeader(OpRepDevlistHeader* header);
OpRepDevlist UnpackOpRepDevlist(SmartBuffer* buf);
OpRepDevlistHeader UnpackOpRepDevlistHeader(SmartBuffer* buf);

#endif  // VIRTUAL_USB_DEVICE_OP_COMMANDS_H_
