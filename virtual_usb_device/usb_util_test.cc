// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_util.h"

#include <gtest/gtest.h>

#include "virtual-usb-printer/common/smart_buffer.h"

bool operator==(const Urb& lhs, const Urb& rhs) {
  return lhs.devid == rhs.devid && lhs.direction == rhs.direction &&
         lhs.ep == rhs.ep && lhs.transfer_flags == rhs.transfer_flags &&
         lhs.transfer_buffer_length == rhs.transfer_buffer_length &&
         lhs.start_frame == rhs.start_frame &&
         lhs.num_of_packets == rhs.num_of_packets &&
         lhs.interval == rhs.interval && lhs.setup == rhs.setup;
}

bool operator==(const UrbReply& lhs, const UrbReply& rhs) {
  return lhs.devid == rhs.devid && lhs.direction == rhs.direction &&
         lhs.ep == rhs.ep && lhs.actual_size == rhs.actual_size &&
         lhs.stalled == rhs.stalled;
}

TEST(UsbUtil, PackUnpackUrb) {
  Urb urb1(65537,                // devid
           1,                    // direction
           0,                    // ep
           512,                  // transfer_flags
           64,                   // transfer_buffer_length
           0,                    // start_frame
           0,                    // num_of_packets
           0,                    // interval
           0x8006000100004000);  // setup

  SmartBuffer packed = PackUrb(urb1);
  Urb urb2 = UnpackUrb(&packed);
  EXPECT_EQ(urb1, urb2);
  EXPECT_EQ(packed.size(), 0);
}

TEST(UsbUtil, PackUnpackUrbReply) {
  UrbReply urbreply1(65537,  // devid
                     1,      // direction
                     0,      // ep
                     18,     // actual_size
                     1);     // stalled

  SmartBuffer packed = PackUrbReply(urbreply1);
  UrbReply urbreply2 = UnpackUrbReply(&packed);
  EXPECT_EQ(urbreply1, urbreply2);
  EXPECT_EQ(packed.size(), 0);
}
