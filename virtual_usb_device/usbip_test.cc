// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip.h"

#include <gtest/gtest.h>

#include "usb_util.h"
#include "virtual-usb-printer/common/smart_buffer.h"

bool operator==(const UsbipCmdSubmit& lhs, const UsbipCmdSubmit& rhs) {
  return lhs.header.command == rhs.header.command &&
         lhs.header.seqnum == rhs.header.seqnum &&
         lhs.header.devid == rhs.header.devid &&
         lhs.header.direction == rhs.header.direction &&
         lhs.header.ep == rhs.header.ep &&
         lhs.transfer_flags == rhs.transfer_flags &&
         lhs.transfer_buffer_length == rhs.transfer_buffer_length &&
         lhs.start_frame == rhs.start_frame &&
         lhs.number_of_packets == rhs.number_of_packets &&
         lhs.interval == rhs.interval && lhs.setup == rhs.setup;
}

bool operator==(const UsbipRetSubmit& lhs, const UsbipRetSubmit& rhs) {
  return lhs.header.command == rhs.header.command &&
         lhs.header.seqnum == rhs.header.seqnum &&
         lhs.header.devid == rhs.header.devid &&
         lhs.header.direction == rhs.header.direction &&
         lhs.header.ep == rhs.header.ep && lhs.status == rhs.status &&
         lhs.actual_length == rhs.actual_length &&
         lhs.start_frame == rhs.start_frame &&
         lhs.number_of_packets == rhs.number_of_packets &&
         lhs.error_count == rhs.error_count && lhs.setup == rhs.setup;
}

TEST(UsbIpTests, PackUnpackUsbipCmdSubmit) {
  UsbipCmdSubmit cmd1{
      {.command = 1, .seqnum = 3, .devid = 65537, .direction = 1, .ep = 0},
      .transfer_flags = 512,
      .transfer_buffer_length = 64,
      .start_frame = 0,
      .number_of_packets = 0,
      .interval = 0,
      .setup = 0x8006000100004000};

  SmartBuffer packed = PackUsbipCmdSubmit(cmd1);
  UsbipCmdSubmit cmd2 = UnpackUsbipCmdSubmit(&packed);
  EXPECT_EQ(cmd1, cmd2);
  EXPECT_EQ(packed.size(), 0);
}

TEST(UsbIpTests, PackUnpackUsbipRetSubmit) {
  UsbipRetSubmit cmd1{
      {.command = 1, .seqnum = 3, .devid = 65537, .direction = 1, .ep = 0},
      .status = 1,
      .actual_length = 64,
      .start_frame = 0,
      .number_of_packets = 0,
      .error_count = 0,
      .setup = 0x8006000100004000};

  SmartBuffer packed = PackUsbipRetSubmit(cmd1);
  UsbipRetSubmit cmd2 = UnpackUsbipRetSubmit(&packed);
  EXPECT_EQ(cmd1, cmd2);
  EXPECT_EQ(packed.size(), 0);
}

TEST(UsbIpTests, UsbipToUrb) {
  UsbipCmdSubmit cmd1{
      {.command = 1, .seqnum = 3, .devid = 65537, .direction = 1, .ep = 0},
      .transfer_flags = 512,
      .transfer_buffer_length = 64,
      .start_frame = 0,
      .number_of_packets = 0,
      .interval = 0,
      .setup = 0x8006000100004000};

  Urb urb;
  usbip_to_urb(&cmd1, &urb);

  EXPECT_EQ(urb.devid, 65537);
  EXPECT_EQ(urb.direction, 1);
  EXPECT_EQ(urb.ep, 0);
  EXPECT_EQ(urb.transfer_flags, 512);
  EXPECT_EQ(urb.transfer_buffer_length, 64);
  EXPECT_EQ(urb.start_frame, 0);
  EXPECT_EQ(urb.num_of_packets, 0);
  EXPECT_EQ(urb.interval, 0);
  EXPECT_EQ(urb.setup, 0x8006000100004000);
}
