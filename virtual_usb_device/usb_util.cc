// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_util.h"

#include <arpa/inet.h>

#include <cstdint>
#include <string>

#include <base/logging.h>
#include <base/notreached.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/virtual_usb_device/usbip_constants.h"

int GetControlDirection(uint8_t bmRequestType) {
  // Data transfer direction is bit 7.
  return (bmRequestType >> 7) & 0x01;
}

int GetControlType(uint8_t bmRequestType) {
  // The "type" of the request is stored within bits 5 and 6 of `bmRequestType`.
  return (bmRequestType >> 5) & 3;
}

int GetControlRecipient(uint8_t bmRequestType) {
  // The "recipient" of the request is stored in bits 0..4.
  return bmRequestType & 0x1f;
}

std::string RequestDirectionString(uint8_t bmRequestType) {
  return GetControlDirection(bmRequestType) ? "To host" : "To device";
}

std::string RequestTypeString(uint8_t bmRequestType) {
  switch (GetControlType(bmRequestType)) {
    case STANDARD_TYPE:
      return "Standard";
    case CLASS_TYPE:
      return "Class";
    case VENDOR_TYPE:
      return "Vendor";
    case RESERVED_TYPE:
      return "Reserved";
    default:
      NOTREACHED();
  }
}

std::string RequestRecipientString(uint8_t bmRequestType) {
  switch (GetControlRecipient(bmRequestType)) {
    case RECIPIENT_DEVICE:
      return "Device";
    case RECIPIENT_INTERFACE:
      return "Interface";
    case RECIPIENT_ENDPOINT:
      return "Endpoint";
    case RECIPIENT_OTHER:
      return "Other";
    default:
      return "Reserved";
  }
}

std::string StandardDeviceRequestString(uint8_t bRequest) {
  switch (bRequest) {
    case GET_STATUS:
      return "GET_STATUS";
    case CLEAR_FEATURE:
      return "CLEAR_FEATURE";
    case SET_FEATURE:
      return "SET_FEATURE";
    case SET_ADDRESS:
      return "SET_ADDRESS";
    case GET_DESCRIPTOR:
      return "GET_DESCRIPTOR";
    case SET_DESCRIPTOR:
      return "SET_DESCRIPTOR";
    case GET_CONFIGURATION:
      return "GET_CONFIGURATION";
    case SET_CONFIGURATION:
      return "SET_CONFIGURATION";
    case GET_INTERFACE:
      return "GET_INTERFACE";
    case SET_INTERFACE:
      return "SET_INTERFACE";
    case SET_FRAME:
      return "SET_FRAME";
    default:
      LOG(ERROR) << "Unknown standard device bRequest value "
                 << base::StringPrintf("0x%02X", bRequest);
      return base::StringPrintf("BREQUEST_RAW_%02X", bRequest);
  }
}

std::string RequestNameString(uint8_t bmRequestType, uint8_t bRequest) {
  if (GetControlType(bmRequestType) == STANDARD_TYPE) {
    switch (GetControlRecipient(bmRequestType)) {
      case RECIPIENT_DEVICE:
        return StandardDeviceRequestString(bRequest);

        // TODO(b/238353195): Add mappings for known device class requests.
    }
  }

  LOG(ERROR) << "No mapping defined for bmRequestType "
             << base::StringPrintf("0x%02X", bmRequestType);
  return base::StringPrintf("%02X", bRequest);
}

std::string DescriptorTypeString(uint8_t wValue) {
  switch (wValue) {
    case USB_DESCRIPTOR_DEVICE:
      return "USB_DESCRIPTOR_DEVICE";
    case USB_DESCRIPTOR_CONFIGURATION:
      return "USB_DESCRIPTOR_CONFIGURATION";
    case USB_DESCRIPTOR_STRING:
      return "USB_DESCRIPTOR_STRING";
    case USB_DESCRIPTOR_INTERFACE:
      return "USB_DESCRIPTOR_INTERFACE";
    case USB_DESCRIPTOR_ENDPOINT:
      return "USB_DESCRIPTOR_ENDPOINT";
    case USB_DESCRIPTOR_DEVICE_QUALIFIER:
      return "USB_DESCRIPTOR_DEVICE_QUALIFIER";
    case USB_DESCRIPTOR_OTHER_SPEED:
      return "USB_DESCRIPTOR_OTHER_SPEED";
    case USB_DESCRIPTOR_INTERFACE_POWER:
      return "USB_DESCRIPTOR_INTERFACE_POWER";
    case USB_DESCRIPTOR_OTG:
      return "USB_DESCRIPTOR_OTG";
    case USB_DESCRIPTOR_DEBUG:
      return "USB_DESCRIPTOR_DEBUG";
    default:
      LOG(ERROR) << "Unknown descriptor type request: "
                 << base::StringPrintf("0x%02X", wValue);
      return base::StringPrintf("USB_DESCRIPTOR_RAW_%02X", wValue);
  }
}

// Unpacks the standard USB SETUP packet contained within `setup` into a
// UsbControlRequest struct and returns the result.
UsbControlRequest CreateUsbControlRequest(uint64_t setup) {
  UsbControlRequest request;
  request.bmRequestType = (setup & REQUEST_TYPE_MASK) >> 56;
  request.bRequest = (setup & REQUEST_MASK) >> 48;
  request.wValue0 = (setup & VALUE_0_MASK) >> 40;
  request.wValue1 = (setup & VALUE_1_MASK) >> 32;
  request.wIndex0 = (setup & INDEX_0_MASK) >> 24;
  request.wIndex1 = (setup & INDEX_1_MASK) >> 16;
  request.wLength = ntohs(setup & LENGTH_MASK);

  VLOG(2) << "SETUP " << base::StringPrintf("0x%016" PRIX64, setup);

  VLOG(2) << "bmRequestType "
          << base::StringPrintf("0x%02X", request.bmRequestType);
  VLOG(2) << "  direction   " << GetControlDirection(request.bmRequestType)
          << " " << RequestDirectionString(request.bmRequestType);
  VLOG(2) << "  type        " << GetControlType(request.bmRequestType) << " "
          << RequestTypeString(request.bmRequestType);
  VLOG(2) << "  recipient   " << GetControlRecipient(request.bmRequestType)
          << " " << RequestRecipientString(request.bmRequestType);
  VLOG(2) << "bRequest      " << base::StringPrintf("0x%02X", request.bRequest)
          << " " << RequestNameString(request.bmRequestType, request.bRequest);
  VLOG(2) << "wValue0       " << base::StringPrintf("0x%02X", request.wValue0);
  VLOG(2) << "wValue1       " << base::StringPrintf("0x%02X", request.wValue1);
  VLOG(2) << "wIndex0       " << base::StringPrintf("0x%02X", request.wIndex0);
  VLOG(2) << "wIndex1       " << base::StringPrintf("0x%02X", request.wIndex1);
  VLOG(2) << "wLength       " << unsigned{request.wLength};

  return request;
}

void PrintUsbControlRequest(const UsbControlRequest& request) {
  VLOG(2) << "  UC Request Type " << unsigned{request.bmRequestType};
  VLOG(2) << "  UC Request " << unsigned{request.bRequest};
  VLOG(2) << "  UC Value  " << unsigned{request.wValue1} << "["
          << unsigned{request.wValue0} << "]";
  VLOG(2) << "  UC Index  " << unsigned{request.wIndex1} << "-"
          << unsigned{request.wIndex0};
  VLOG(2) << "  UC Length " << unsigned{request.wLength};
}

Urb UnpackUrb(SmartBuffer* buf) {
  Urb result;
  CHECK(buf->size() >= sizeof(result));
  memcpy(&result, buf->data(), sizeof(result));
  buf->Erase(0, sizeof(result));

  result.devid = ntohl(result.devid);
  result.direction = ntohl(result.direction);
  result.ep = ntohl(result.ep);

  result.transfer_flags = ntohl(result.transfer_flags);
  result.transfer_buffer_length = ntohl(result.transfer_buffer_length);
  result.start_frame = ntohl(result.start_frame);
  result.num_of_packets = ntohl(result.num_of_packets);
  result.interval = ntohl(result.interval);
  result.setup = be64toh(result.setup);
  return result;
}

SmartBuffer PackUrb(const Urb& urb) {
  SmartBuffer result;
  result.Add(htonl(urb.devid));
  result.Add(htonl(urb.direction));
  result.Add(htonl(urb.ep));

  result.Add(htonl(urb.transfer_flags));
  result.Add(htonl(urb.transfer_buffer_length));
  result.Add(htonl(urb.start_frame));
  result.Add(htonl(urb.num_of_packets));
  result.Add(htonl(urb.interval));

  result.Add(htobe64(urb.setup));
  return result;
}

void PrintUrb(const Urb& req) {
  VLOG(2) << "devid: " << req.devid;
  VLOG(2) << "direction: " << req.direction;
  VLOG(2) << "ep: " << req.ep;
  VLOG(2) << "transfer_flags: " << req.transfer_flags;
  VLOG(2) << "transfer_buffer_length: " << req.transfer_buffer_length;
  VLOG(2) << "start_frame: " << req.start_frame;
  VLOG(2) << "num_of_packets: " << req.num_of_packets;
  VLOG(2) << "interval: " << req.interval;
  VLOG(2) << "setup: " << base::StringPrintf("0x%016" PRIX64, req.setup);
}

SmartBuffer PackUrbReply(const UrbReply& urb) {
  SmartBuffer result;
  result.Add(htonl(urb.devid));
  result.Add(htonl(urb.direction));
  result.Add(htonl(urb.ep));
  result.Add(htonl(urb.actual_size));
  result.Add(htonl(urb.stalled));
  return result;
}

UrbReply UnpackUrbReply(SmartBuffer* buf) {
  UrbReply result;
  CHECK(buf->size() >= sizeof(result));
  memcpy(&result, buf->data(), sizeof(result));
  buf->Erase(0, sizeof(result));

  result.devid = ntohl(result.devid);
  result.direction = ntohl(result.direction);
  result.ep = ntohl(result.ep);

  result.actual_size = ntohl(result.actual_size);
  result.stalled = ntohl(result.stalled);
  return result;
}
