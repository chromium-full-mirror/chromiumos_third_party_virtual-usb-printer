// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_descriptors.h"

#include <arpa/inet.h>
#include <cstdio>
#include <cstring>

#include <base/check.h>

#include "virtual-usb-printer/common/smart_buffer.h"

SmartBuffer PackUsbDescriptor(UsbDeviceDescriptor& data) {
  SmartBuffer serialized;
  serialized.Add(data.bLength);
  serialized.Add(data.bDescriptorType);
  serialized.Add(htons(data.bcdUSB));
  serialized.Add(data.bDeviceClass);
  serialized.Add(data.bDeviceSubClass);
  serialized.Add(data.bDeviceProtocol);
  serialized.Add(data.bMaxPacketSize0);
  serialized.Add(htons(data.idVendor));
  serialized.Add(htons(data.idProduct));
  serialized.Add(htons(data.bcdDevice));
  serialized.Add(data.iManufacturer);
  serialized.Add(data.iProduct);
  serialized.Add(data.iSerialNumber);
  serialized.Add(data.bNumConfigurations);
  return serialized;
}

SmartBuffer PackConfigDescriptor(UsbConfigurationDescriptor& data) {
  SmartBuffer serialized;
  serialized.Add(data.bLength);
  serialized.Add(data.bDescriptorType);
  serialized.Add(htons(data.wTotalLength));
  serialized.Add(data.bNumInterfaces);
  serialized.Add(data.bConfigurationValue);
  serialized.Add(data.iConfiguration);
  serialized.Add(data.bmAttributes);
  serialized.Add(data.bMaxPower);
  return serialized;
}

SmartBuffer PackQualifierDescriptor(UsbDeviceQualifierDescriptor& data) {
  SmartBuffer serialized;
  serialized.Add(data.bLength);
  serialized.Add(data.bDescriptorType);
  serialized.Add(htons(data.bcdUSB));
  serialized.Add(data.bDeviceClass);
  serialized.Add(data.bDeviceSubClass);
  serialized.Add(data.bDeviceProtocol);
  serialized.Add(data.bMaxPacketSize0);
  serialized.Add(data.bNumConfigurations);
  serialized.Add(data.bReserved);
  return serialized;
}

SmartBuffer PackInterfaceDescriptor(UsbInterfaceDescriptor& data) {
  SmartBuffer packed;
  packed.Add(data);
  return packed;
}

UsbDeviceDescriptor UnpackUsbDescriptor(SmartBuffer& buf) {
  UsbDeviceDescriptor result;
  CHECK(buf.size() >= sizeof(result));
  memcpy(&result, buf.data(), sizeof(result));
  buf.Erase(0, sizeof(result));

  result.bcdUSB = ntohs(result.bcdUSB);
  result.idVendor = ntohs(result.idVendor);
  result.idProduct = ntohs(result.idProduct);
  result.bcdDevice = ntohs(result.bcdDevice);
  return result;
}

UsbConfigurationDescriptor UnpackConfigDescriptor(SmartBuffer& buf) {
  UsbConfigurationDescriptor result;
  CHECK(buf.size() >= sizeof(result));
  memcpy(&result, buf.data(), sizeof(result));
  buf.Erase(0, sizeof(result));

  result.wTotalLength = ntohs(result.wTotalLength);
  return result;
}

UsbDeviceQualifierDescriptor UnpackQualifierDescriptor(SmartBuffer& buf) {
  UsbDeviceQualifierDescriptor result;
  CHECK(buf.size() >= sizeof(result));
  memcpy(&result, buf.data(), sizeof(result));
  buf.Erase(0, sizeof(result));

  result.bcdUSB = ntohs(result.bcdUSB);
  return result;
}

UsbInterfaceDescriptor UnpackInterfaceDescriptors(SmartBuffer& buf) {
  UsbInterfaceDescriptor result;
  CHECK(buf.size() >= sizeof(result));
  memcpy(&result, buf.data(), sizeof(result));
  buf.Erase(0, sizeof(result));

  return result;
}

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
