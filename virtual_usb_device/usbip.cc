// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip.h"

#include <cinttypes>
#include <arpa/inet.h>

#include <base/check.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "device_descriptors.h"
#include "server.h"
#include "usbip_constants.h"
#include "usb_util.h"
#include "virtual-usb-printer/common/smart_buffer.h"

UsbipRetSubmit UnpackUsbipRetSubmit(SmartBuffer* buf) {
  UsbipRetSubmit result;
  LOG(INFO) << "buf->size() " << buf->size();
  LOG(INFO) << "sizeof UsbipRetSubmit " << sizeof(result);
  CHECK(buf->size() >= sizeof(result));
  memcpy(&result, buf->data(), sizeof(result));
  buf->Erase(0, sizeof(result));

  result.header.command = ntohl(result.header.command);
  result.header.seqnum = ntohl(result.header.seqnum);
  result.header.devid = ntohl(result.header.devid);
  result.header.direction = ntohl(result.header.direction);
  result.header.ep = ntohl(result.header.ep);

  result.status = ntohl(result.status);
  result.actual_length = ntohl(result.actual_length);
  result.start_frame = ntohl(result.start_frame);
  result.number_of_packets = ntohl(result.number_of_packets);
  result.error_count = ntohl(result.error_count);
  result.setup = be64toh(result.setup);

  return result;
}

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

SmartBuffer PackUsbipCmdSubmit(const UsbipCmdSubmit& cmd) {
  SmartBuffer serialized(sizeof(cmd));
  serialized.Add(htonl(cmd.header.command));
  serialized.Add(htonl(cmd.header.seqnum));
  serialized.Add(htonl(cmd.header.devid));
  serialized.Add(htonl(cmd.header.direction));
  serialized.Add(htonl(cmd.header.ep));

  serialized.Add(htonl(cmd.transfer_flags));
  serialized.Add(htonl(cmd.transfer_buffer_length));
  serialized.Add(htonl(cmd.start_frame));
  serialized.Add(htonl(cmd.number_of_packets));
  serialized.Add(htonl(cmd.interval));
  serialized.Add(htobe64(cmd.setup));

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

void usbip_to_urb(const struct UsbipCmdSubmit* usbip, struct Urb* urb) {
  urb->devid = usbip->header.devid;
  urb->direction = usbip->header.direction;
  urb->ep = usbip->header.ep;
  urb->transfer_flags = usbip->transfer_flags;
  urb->transfer_buffer_length = usbip->transfer_buffer_length;
  urb->start_frame = usbip->start_frame;
  urb->num_of_packets = usbip->number_of_packets;
  urb->interval = usbip->interval;
  urb->setup = usbip->setup;
}
