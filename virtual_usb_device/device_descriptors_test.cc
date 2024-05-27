// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_descriptors.h"

#include <gtest/gtest.h>

#include "virtual-usb-printer/common/smart_buffer.h"

TEST(DeviceDescriptorTest, PackUnpackUsbDeviceDescriptors) {
  UsbDeviceDescriptor descriptor1(18,     // bLength
                                  1,      // bDescriptorType
                                  272,    // bcdUSB
                                  0,      // bDeviceClass
                                  0,      // bDeviceSubClass
                                  0,      // bDeviceProtocol
                                  8,      // bMaxPacketSize0
                                  6353,   // idVendor
                                  20574,  // idProduct
                                  0,      // bcdDevice
                                  1,      // iManufacturer
                                  2,      // iProduct
                                  1,      // iSerialNumber
                                  1);     // bNumConfigurations

  SmartBuffer packed_descriptor = PackUsbDescriptor(descriptor1);
  UsbDeviceDescriptor descriptor2 = UnpackUsbDescriptor(packed_descriptor);
  EXPECT_EQ(descriptor1, descriptor2);
  EXPECT_EQ(packed_descriptor.size(), 0);
}

TEST(DeviceDescriptorTest, PackUnpackConfigurationDescriptor) {
  UsbConfigurationDescriptor descriptor1(9,    // bLength
                                         2,    // bDescriptorType
                                         32,   // wTotalLength
                                         1,    // bNumInterfaces
                                         1,    // bConfigurationValue
                                         0,    // iConfiguration
                                         128,  // bmAttributes
                                         0);   // bMaxPower

  SmartBuffer packed_descriptor = PackConfigDescriptor(descriptor1);
  UsbConfigurationDescriptor descriptor2 =
      UnpackConfigDescriptor(packed_descriptor);
  EXPECT_EQ(descriptor1, descriptor2);
  EXPECT_EQ(packed_descriptor.size(), 0);
}

TEST(DeviceDescriptorTest, PackUnpackQualifierDescriptor) {
  UsbDeviceQualifierDescriptor descriptor1(10,   // bLength
                                           6,    // bDescriptorType
                                           272,  // bcdUSB
                                           0,    // bDeviceClass
                                           0,    // bDeviceSubClass
                                           0,    // bDeviceProtocol
                                           255,  // bMaxPacketSize0
                                           1,    // bNumConfigurations
                                           0);   // bReserved

  SmartBuffer packed_descriptor = PackQualifierDescriptor(descriptor1);
  UsbDeviceQualifierDescriptor descriptor2 =
      UnpackQualifierDescriptor(packed_descriptor);
  EXPECT_EQ(descriptor1, descriptor2);
  EXPECT_EQ(packed_descriptor.size(), 0);
}

TEST(DeviceDescriptorTest, PackUnpackInterfaceDescriptors) {
  UsbInterfaceDescriptor descriptor1(9,   // bLength
                                     4,   // bDescriptorType
                                     0,   // bInterfaceNumber
                                     0,   // bAlternateSetting
                                     2,   // bNumEndpoints
                                     7,   // bInterfaceClass
                                     1,   // bInterfaceSubClass
                                     2,   // bInterfaceProtocol
                                     0);  // iInterface
  SmartBuffer packed_descriptor = PackInterfaceDescriptor(descriptor1);
  UsbInterfaceDescriptor descriptor2 =
      UnpackInterfaceDescriptors(packed_descriptor);
  EXPECT_EQ(descriptor1, descriptor2);
  EXPECT_EQ(packed_descriptor.size(), 0);
}
