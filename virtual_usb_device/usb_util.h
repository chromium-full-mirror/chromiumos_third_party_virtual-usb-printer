// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USB_UTIL_H_
#define VIRTUAL_USB_DEVICE_USB_UTIL_H_

#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <base/check.h>
#include <base/check_op.h>

#include "usbip_constants.h"
#include "virtual-usb-printer/common/smart_buffer.h"

// Bitmask constants used for extracting individual values out of
// UsbControlRequest which is packed inside of a uint64_t.
const uint64_t REQUEST_TYPE_MASK{0xFFULL << 56};
const uint64_t REQUEST_MASK{0xFFULL << 48};
const uint64_t VALUE_0_MASK{0xFFULL << 40};
const uint64_t VALUE_1_MASK{0xFFULL << 32};
const uint64_t INDEX_0_MASK{0xFFUL << 24};
const uint64_t INDEX_1_MASK{0xFFUL << 16};
const uint64_t LENGTH_MASK{0xFFFFUL};

// Represents a USB SETUP packet.
struct UsbControlRequest {
  uint8_t bmRequestType;
  uint8_t bRequest;
  uint8_t wValue0;
  uint8_t wValue1;
  uint8_t wIndex0;
  uint8_t wIndex1;
  uint16_t wLength;
} __attribute__((packed));

struct Urb {
  uint32_t devid;                   // Specifies a remote USB devie uniquely.
  uint32_t direction;               // Direction of the transfer (0 Out, 1 In).
  uint32_t ep;                      // USB endpoint number
  uint32_t transfer_flags;          // URB flags.
  uint32_t transfer_buffer_length;  // Data size for transfer.
  uint32_t start_frame;     // Initial frame for iso or interrupt transfers.
  uint32_t num_of_packets;  // Number of iso packets.
  uint32_t interval;        // Timeout for response.
  uint64_t setup;           // Contains a USB SETUP packet.
} __attribute__((packed));

struct UrbReply {
  uint32_t devid;        // Specifies a remote USB device uniquely.
  uint32_t direction;    // Direction of the transfer (0 Out, 1 In).
  uint32_t ep;           // USB endpoint number
  uint32_t actual_size;  // actual size of replied data.
  uint32_t stalled;      // whether the command was stalled.
} __attribute__((packed));

SmartBuffer PackUrb(const Urb& urb);
Urb UnpackUrb(SmartBuffer* buf);

SmartBuffer PackUrbReply(const UrbReply& urb);
UrbReply UnpackUrbReply(SmartBuffer* buf);

// Unpacks the standard USB SETUP packet contained within `setup` into a
// UsbControlRequest struct and returns the result.
UsbControlRequest CreateUsbControlRequest(uint64_t setup);

// Returns the numeric value of the "direction" bit within `bmRequestType`.
int GetControlDirection(uint8_t bmRequestType);

// Returns the numeric value of the "type" stored within the `bmRequestType`
// bitmap.
int GetControlType(uint8_t bmRequestType);

// Returns the numeric value of the "recipient" stored within `bmRequestType`.
int GetControlRecipient(uint8_t bmRequestType);

// The below function are used for logging purposes.

// Return usb data transfer direction as string.
// which is either Host to Device or Device to Host.
std::string RequestDirectionString(uint8_t bmRequestType);
// Return type of usb request as string.
// There are four type of request: Standard, class, vendor, reserved.
std::string RequestTypeString(uint8_t bmRequestType);
// Return recipient of usb request as string.
// which could be among Device, Interface, Endpoint and Other.
std::string RequestRecipientString(uint8_t bmRequestType);
// Return standard usb request name as string.
std::string StandardDeviceRequestString(uint8_t bRequest);
std::string RequestNameString(uint8_t bmRequestType, uint8_t bRequest);
// Return descriptor type as string.
// There are four type: device descriptor, configuration descriptor, string
// descriptor, interface descritor.
std::string DescriptorTypeString(uint8_t wValue);

void PrintUsbControlRequest(const UsbControlRequest& request);

#endif  // VIRTUAL_USB_DEVICE_USB_UTIL_H_
