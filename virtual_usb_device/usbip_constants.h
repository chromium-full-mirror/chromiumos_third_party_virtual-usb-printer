// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USBIP_CONSTANTS_H_
#define VIRTUAL_USB_DEVICE_USBIP_CONSTANTS_H_

// USB Descriptor Type Constants.
#define USB_DESCRIPTOR_DEVICE 0x01            // Device Descriptor.
#define USB_DESCRIPTOR_CONFIGURATION 0x02     // Configuration Descriptor.
#define USB_DESCRIPTOR_STRING 0x03            // String Descriptor.
#define USB_DESCRIPTOR_INTERFACE 0x04         // Interface Descriptor.
#define USB_DESCRIPTOR_ENDPOINT 0x05          // Endpoint Descriptor.
#define USB_DESCRIPTOR_DEVICE_QUALIFIER 0x06  // Device Qualifier.
#define USB_DESCRIPTOR_OTHER_SPEED 0x07       // Other speed configuration.
#define USB_DESCRIPTOR_INTERFACE_POWER 0x08   // Interface power descriptor.
#define USB_DESCRIPTOR_OTG 0x09               // USB on-the-go descriptor.
#define USB_DESCRIPTOR_DEBUG 0x0A             // Debug descriptor.

#define STANDARD_TYPE 0  // Standard USB Request.
#define CLASS_TYPE 1     // Class-specific USB Request.
#define VENDOR_TYPE 2    // Vendor-specific USB Request.
#define RESERVED_TYPE 3  // Reserved.

#define RECIPIENT_DEVICE 0x00     // Request directed at whole device.
#define RECIPIENT_INTERFACE 0x01  // Request directed at specific interface.
#define RECIPIENT_ENDPOINT 0x02   // Request directed at specific endpoint.
#define RECIPIENT_OTHER 0x03      // Request directed somewhere else.

// USB "bRequest" Constants.
// These represent the possible values contained within a USB SETUP packet which
// specify the type of request.
#define GET_STATUS 0x00
#define CLEAR_FEATURE 0x01
#define SET_FEATURE 0x03
#define SET_ADDRESS 0x05
#define GET_DESCRIPTOR 0x06
#define SET_DESCRIPTOR 0x07
#define GET_CONFIGURATION 0x08
#define SET_CONFIGURATION 0x09
#define GET_INTERFACE 0x0A
#define SET_INTERFACE 0x0B
#define SET_FRAME 0x0C

// Special "bRequest" values for printer requests.
#define GET_DEVICE_ID 0
#define GET_PORT_STATUS 1
#define SOFT_RESET 2
#define GET_MAX_LUN 254

// OP Commands.
#define OP_REQ_IMPORT_CMD 0x8003
#define OP_REP_IMPORT_CMD 0x0003
#define OP_REQ_DEVLIST_CMD 0x8005
#define OP_REP_DEVLIST_CMD 0x0005

// usbip command sent by device for exporting/binding ( see usbip bind)
// These are custom message and not defined by usbip protocol.
#define OP_REQ_BIND_CMD 0x8007
#define OP_REP_BIND_CMD 0x0007

#define OP_REQ_UNBIND_CMD 0x8009

// USBIP Command Constants.
#define COMMAND_USBIP_CMD_SUBMIT 0x0001
#define COMMAND_USBIP_CMD_UNLINK 0X0002
#define COMMAND_USBIP_RET_SUBMIT 0x0003
#define COMMAND_USBIP_RET_UNLINK 0x0004

// IPP Operation IDs.
#define IPP_VALIDATE_JOB 0x0004
#define IPP_CREATE_JOB 0x0005
#define IPP_SEND_DOCUMENT 0x0006
#define IPP_GET_JOB_ATTRIBUTES 0x0009
#define IPP_GET_PRINTER_ATTRIBUTES 0x000B

#endif  // VIRTUAL_USB_DEVICE_USBIP_CONSTANTS_H_
