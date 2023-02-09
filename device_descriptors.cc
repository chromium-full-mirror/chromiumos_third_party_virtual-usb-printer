// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_descriptors.h"

#include <cstdio>

bool operator==(const UsbDeviceDescriptor& lhs,
                const UsbDeviceDescriptor& rhs) {
  return lhs.bLength == rhs.bLength &&
         lhs.bDescriptorType == rhs.bDescriptorType &&
         lhs.bcdUSB == rhs.bcdUSB && lhs.bDeviceClass == rhs.bDeviceClass &&
         lhs.bDeviceSubClass == rhs.bDeviceSubClass &&
         lhs.bDeviceProtocol == rhs.bDeviceProtocol &&
         lhs.bMaxPacketSize0 == rhs.bMaxPacketSize0 &&
         lhs.idVendor == rhs.idVendor && lhs.idProduct == rhs.idProduct &&
         lhs.bcdDevice == rhs.bcdDevice &&
         lhs.iManufacturer == rhs.iManufacturer &&
         lhs.iProduct == rhs.iProduct &&
         lhs.iSerialNumber == rhs.iSerialNumber &&
         lhs.bNumConfigurations == rhs.bNumConfigurations;
}

bool operator==(const UsbConfigurationDescriptor& lhs,
                const UsbConfigurationDescriptor& rhs) {
  return lhs.bLength == rhs.bLength &&
         lhs.bDescriptorType == rhs.bDescriptorType &&
         lhs.wTotalLength == rhs.wTotalLength &&
         lhs.bNumInterfaces == rhs.bNumInterfaces &&
         lhs.bConfigurationValue == rhs.bConfigurationValue &&
         lhs.iConfiguration == rhs.iConfiguration &&
         lhs.bmAttributes == rhs.bmAttributes && lhs.bMaxPower == rhs.bMaxPower;
}

bool operator==(const UsbInterfaceDescriptor& lhs,
                const UsbInterfaceDescriptor& rhs) {
  return lhs.bLength == rhs.bLength &&
         lhs.bDescriptorType == rhs.bDescriptorType &&
         lhs.bInterfaceNumber == rhs.bInterfaceNumber &&
         lhs.bAlternateSetting == rhs.bAlternateSetting &&
         lhs.bNumEndpoints == rhs.bNumEndpoints &&
         lhs.bInterfaceClass == rhs.bInterfaceClass &&
         lhs.bInterfaceSubClass == rhs.bInterfaceSubClass &&
         lhs.bInterfaceProtocol == rhs.bInterfaceProtocol &&
         lhs.iInterface == rhs.iInterface;
}

bool operator==(const UsbEndpointDescriptor& lhs,
                const UsbEndpointDescriptor& rhs) {
  return lhs.bLength == rhs.bLength &&
         lhs.bDescriptorType == rhs.bDescriptorType &&
         lhs.bEndpointAddress == rhs.bEndpointAddress &&
         lhs.bmAttributes == rhs.bmAttributes &&
         lhs.wMaxPacketSize == rhs.wMaxPacketSize &&
         lhs.bInterval == rhs.bInterval;
}

bool operator==(const UsbDeviceQualifierDescriptor& lhs,
                const UsbDeviceQualifierDescriptor& rhs) {
  return lhs.bLength == rhs.bLength &&
         lhs.bDescriptorType == rhs.bDescriptorType &&
         lhs.bcdUSB == rhs.bcdUSB && lhs.bDeviceClass == rhs.bDeviceClass &&
         lhs.bDeviceSubClass == rhs.bDeviceSubClass &&
         lhs.bDeviceProtocol == rhs.bDeviceProtocol &&
         lhs.bMaxPacketSize0 == rhs.bMaxPacketSize0 &&
         lhs.bNumConfigurations == rhs.bNumConfigurations &&
         lhs.bReserved == rhs.bReserved;
}
