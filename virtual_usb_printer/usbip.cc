// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip.h"

#include <cinttypes>

#include "device_descriptors.h"
#include "server.h"
#include "smart_buffer.h"
#include "usb_printer.h"
#include "usbip_constants.h"

#include <base/check.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

SmartBuffer PackUsbipRetSubmit(const UsbipRetSubmit& reply) {
  SmartBuffer serialized;
  serialized.Add(htonl(reply.header.command));
  serialized.Add(htonl(reply.header.seqnum));
  serialized.Add(htonl(reply.header.devid));
  serialized.Add(htonl(reply.header.direction));
  serialized.Add(htonl(reply.header.ep));

  serialized.Add(htonl(reply.status));
  serialized.Add(htonl(reply.actual_length));
  serialized.Add(htonl(reply.start_frame));
  serialized.Add(htonl(reply.number_of_packets));
  serialized.Add(htonl(reply.error_count));

  serialized.Add(htobe64(reply.setup));
  return serialized;
}

UsbipCmdSubmit UnpackUsbipCmdSubmit(SmartBuffer* buf) {
  UsbipCmdSubmit result;
  CHECK(buf->size() >= sizeof(result));
  memcpy(&result, buf->data(), sizeof(result));
  buf->Erase(0, sizeof(result));

  result.header.command = ntohl(result.header.command);
  result.header.seqnum = ntohl(result.header.seqnum);
  result.header.devid = ntohl(result.header.devid);
  result.header.direction = ntohl(result.header.direction);
  result.header.ep = ntohl(result.header.ep);

  result.transfer_flags = ntohl(result.transfer_flags);
  result.transfer_buffer_length = ntohl(result.transfer_buffer_length);
  result.start_frame = ntohl(result.start_frame);
  result.number_of_packets = ntohl(result.number_of_packets);
  result.interval = ntohl(result.interval);
  result.setup = be64toh(result.setup);
  return result;
}

void PrintUsbipHeaderBasic(const UsbipHeaderBasic& header) {
  VLOG(2) << "usbip cmd " << header.command;
  VLOG(2) << "usbip seqnum " << header.seqnum;
  VLOG(2) << "usbip devid " << header.devid;
  VLOG(2) << "usbip direction " << header.direction;
  VLOG(2) << "usbip ep " << header.ep;
}

void PrintUsbipCmdSubmit(const UsbipCmdSubmit& command) {
  VLOG(2) << "== START CMD ==";
  PrintUsbipHeaderBasic(command.header);
  VLOG(2) << "usbip flags " << command.transfer_flags;
  VLOG(2) << "usbip number of packets " << command.number_of_packets;
  VLOG(2) << "usbip interval " << command.interval;
  VLOG(2) << "usbip setup "
          << base::StringPrintf("0x%016" PRIX64, command.setup);
  VLOG(2) << "usbip buffer length " << command.transfer_buffer_length;
  VLOG(2) << "== END CMD ==";
}

void PrintUsbipRetSubmit(const UsbipRetSubmit& response) {
  VLOG(2) << "== START RET ==";
  PrintUsbipHeaderBasic(response.header);
  VLOG(2) << "usbip status " << response.status;
  VLOG(2) << "usbip actual_length " << response.actual_length;
  VLOG(2) << "usbip start_frame " << response.start_frame;
  VLOG(2) << "usbip number_of_packets " << response.number_of_packets;
  VLOG(2) << "usbip error_count " << response.error_count;
  VLOG(2) << "== END RET ==";
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

UsbipRetSubmit CreateUsbipRetSubmit(const UsbipCmdSubmit& request) {
  UsbipRetSubmit response;
  memset(&response, 0, sizeof(response));
  response.header.command = COMMAND_USBIP_RET_SUBMIT;
  response.header.seqnum = request.header.seqnum;
  response.header.devid = request.header.devid;
  response.header.direction = request.header.direction;
  response.header.ep = request.header.ep;
  return response;
}

void SendUsbDataResponse(int sockfd,
                         const UsbipCmdSubmit& usb_request,
                         size_t received) {
  UsbipRetSubmit response = CreateUsbipRetSubmit(usb_request);
  response.actual_length = received;

  PrintUsbipRetSubmit(response);
  SendBuffer(sockfd, PackUsbipRetSubmit(response));
}

void SendUsbControlResponse(int sockfd,
                            const UsbipCmdSubmit& usb_request,
                            const uint8_t* data,
                            size_t data_size) {
  UsbipRetSubmit response = CreateUsbipRetSubmit(usb_request);
  response.actual_length = data_size;

  PrintUsbipRetSubmit(response);
  SmartBuffer smart_buffer = PackUsbipRetSubmit(response);
  if (data_size > 0) {
    smart_buffer.Add(data, data_size);
  }
  SendBuffer(sockfd, smart_buffer);
}
