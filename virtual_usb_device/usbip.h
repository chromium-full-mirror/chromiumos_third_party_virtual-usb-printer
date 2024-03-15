// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USBIP_H_
#define VIRTUAL_USB_DEVICE_USBIP_H_

/*
 * This file defines the supported messages from the usbip-core protocol,
 * and some utility functions for processing them.
 *
 * In the context of the defined messages:
 *   "Cmd" is used in messages that submit a request.
 *   "Ret" is used in messages that respond to a request.
 *
 * For more information about the usbip protocol refer to the following
 * documentation:
 * https://www.kernel.org/doc/Documentation/usb/usbip_protocol.txt
 * https://en.opensuse.org/SDB:USBIP
 */

#include <cstdlib>
#include <vector>

#include "device_descriptors.h"
#include "usbip_constants.h"
#include "virtual-usb-printer/common/smart_buffer.h"

// Common USBIP header used in both requests and responses.
struct UsbipHeaderBasic {
  uint32_t command;    // The USBIP request type.
  uint32_t seqnum;     // Sequential number that identifies requests.
  uint32_t devid;      // Specifies a remote USB devie uniquely.
  uint32_t direction;  // Direction of the transfer (0 Out, 1 In).
  uint32_t ep;         // The USB endpoint number.
} __attribute__((packed));

// Used to submit a USB request.
struct UsbipCmdSubmit {
  UsbipHeaderBasic header;
  uint32_t transfer_flags;          // URB flags.
  uint32_t transfer_buffer_length;  // Data size for transfer.
  uint32_t start_frame;        // Initial frame for iso or interrupt transfers.
  uint32_t number_of_packets;  // Number of iso packets.
  uint32_t interval;           // Timeout for response.
  uint64_t setup;              // Contains a USB SETUP packet.
} __attribute__((packed));

// Used to reply to a USB request.
struct UsbipRetSubmit {
  UsbipHeaderBasic header;
  uint32_t status;  // Response status (O for success, non-zero for error).
  uint32_t actual_length;      // Number of bytes transferred.
  uint32_t start_frame;        // Iniitial frame for iso or interrupt transfers.
  uint32_t number_of_packets;  // Number of iso packets.
  uint32_t error_count;        // Number of errors for iso transfers.
  uint64_t setup;              // Contains a USB SETUP packet.
} __attribute__((packed));

// Populate Urb (USB request block) with usbip request
void usbip_to_urb(const struct UsbipCmdSubmit* usbip, struct Urb* urb);

// Prints the contents of various elements of USBIP messages for debugging
// purposes.
void PrintUsbHeaderBasic(const UsbipHeaderBasic& header);
void PrintUsbipCmdSubmit(const UsbipCmdSubmit& command);
void PrintUsbipRetSubmit(const UsbipRetSubmit& response);

// Creates a new UsbipRetSubmit which is initialized using the shared values
// from `request`.
UsbipRetSubmit CreateUsbipRetSubmit(const UsbipCmdSubmit& usb_request);

// Serializes `reply` into a buffer and converts the contents to network byte
// order.
SmartBuffer PackUsbipRetSubmit(const UsbipRetSubmit& reply);

// Reads a UsbipRetSubmit struct from `buf` and converts the contents of the
// message into host byte order.
//
// Erases the deserialized bytes from `buf`.
UsbipRetSubmit UnpackUsbipRetSubmit(SmartBuffer* buf);

// Serializes `cmd` into a buffer and converts the contents to network byte
// order.
SmartBuffer PackUsbipCmdSubmit(const UsbipCmdSubmit& cmd);

// Reads a UsbipCmdSubmit struct from `buf` and converts the contents of the
// message into host byte order.
//
// Erases the deserialized bytes from `buf`.
UsbipCmdSubmit UnpackUsbipCmdSubmit(SmartBuffer* buf);

#endif  // VIRTUAL_USB_DEVICE_USBIP_H_
