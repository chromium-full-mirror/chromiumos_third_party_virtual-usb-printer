// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_descriptors.h"

#include <cstdio>
#include <arpa/inet.h>

SmartBuffer PackUsbDescriptor(UsbDeviceDescriptor& data) {
  SmartBuffer serialized;
  serialized.Add(htonl(data.bLength));
  serialized.Add(htonl(data.bDescriptorType));
  serialized.Add(htonl(data.bcdUSB));
  serialized.Add(htonl(data.bDeviceClass));
  serialized.Add(htonl(data.bDeviceSubClass));
  serialized.Add(htonl(data.bDeviceProtocol));
  serialized.Add(htonl(data.bMaxPacketSize0));
  serialized.Add(htonl(data.idVendor));
  serialized.Add(htonl(data.idProduct));
  serialized.Add(htonl(data.bcdDevice));
  serialized.Add(htonl(data.iManufacturer));
  serialized.Add(htonl(data.iProduct));
  serialized.Add(htonl(data.iSerialNumber));
  serialized.Add(htonl(data.bNumConfigurations));
  return serialized;
}

SmartBuffer PackConfigDescriptor(UsbConfigurationDescriptor& data) {
  SmartBuffer serialized;
  serialized.Add(htonl(data.bLength));
  serialized.Add(htonl(data.bDescriptorType));
  serialized.Add(htonl(data.wTotalLength));
  serialized.Add(htonl(data.bNumInterfaces));
  serialized.Add(htonl(data.bConfigurationValue));
  serialized.Add(htonl(data.iConfiguration));
  serialized.Add(htonl(data.bmAttributes));
  serialized.Add(htonl(data.bMaxPower));
  return serialized;
}

SmartBuffer PackQualifierDescriptor(UsbDeviceQualifierDescriptor& data) {
  SmartBuffer serialized;
  serialized.Add(htonl(data.bLength));
  serialized.Add(htonl(data.bDescriptorType));
  serialized.Add(htonl(data.bcdUSB));
  serialized.Add(htonl(data.bDeviceClass));
  serialized.Add(htonl(data.bDeviceSubClass));
  serialized.Add(htonl(data.bDeviceProtocol));
  serialized.Add(htonl(data.bMaxPacketSize0));
  serialized.Add(htonl(data.bNumConfigurations));
  return serialized;
}

UsbDeviceDescriptor UnpackUsbDescriptor(SmartBuffer& buf) {
  UsbDeviceDescriptor result;
  CHECK(buf.size() >= sizeof(result));
  memcpy(&result, buf.data(), sizeof(result));
  buf.Erase(0, sizeof(result));

  result.bLength = ntohl(result.bLength);
  result.bDescriptorType = ntohl(result.bDescriptorType);
  result.bcdUSB = ntohl(result.bcdUSB);
  result.bDeviceClass = ntohl(result.bDeviceClass);
  result.bDeviceSubClass = ntohl(result.bDeviceSubClass);
  result.bDeviceProtocol = ntohl(result.bDeviceProtocol);
  result.bMaxPacketSize0 = ntohl(result.bMaxPacketSize0);
  result.idVendor = ntohl(result.idVendor);
  result.idProduct = ntohl(result.idProduct);
  result.bcdDevice = ntohl(result.bcdDevice);
  result.iManufacturer = ntohl(result.iManufacturer);
  result.iProduct = ntohl(result.iProduct);
  result.iSerialNumber = ntohl(result.iSerialNumber);
  result.bNumConfigurations = ntohl(result.bNumConfigurations);
  return result;
}

UsbConfigurationDescriptor UnpackConfigDescriptor(SmartBuffer& buf) {
  UsbConfigurationDescriptor result;
  CHECK(buf.size() >= sizeof(result));
  memcpy(&result, buf.data(), sizeof(result));
  buf.Erase(0, sizeof(result));

  result.bLength = ntohl(result.bLength);
  result.bDescriptorType = ntohl(result.bDescriptorType);
  result.wTotalLength = ntohl(result.wTotalLength);
  result.bNumInterfaces = ntohl(result.bNumInterfaces);
  result.bConfigurationValue = ntohl(result.bConfigurationValue);
  result.iConfiguration = ntohl(result.iConfiguration);
  result.bmAttributes = ntohl(result.bmAttributes);
  result.bMaxPower = ntohl(result.bMaxPower);
  return result;
}

UsbDeviceQualifierDescriptor UnpackQualifierDescriptor(SmartBuffer& buf) {
  UsbDeviceQualifierDescriptor result;
  CHECK(buf.size() >= sizeof(result));
  memcpy(&result, buf.data(), sizeof(result));
  buf.Erase(0, sizeof(result));

  result.bLength = ntohl(result.bLength);
  result.bDescriptorType = ntohl(result.bDescriptorType);
  result.bcdUSB = ntohl(result.bcdUSB);
  result.bDeviceClass = ntohl(result.bDeviceClass);
  result.bDeviceSubClass = ntohl(result.bDeviceSubClass);
  result.bDeviceProtocol = ntohl(result.bDeviceProtocol);
  result.bMaxPacketSize0 = ntohl(result.bMaxPacketSize0);
  result.bNumConfigurations = ntohl(result.bNumConfigurations);
  return result;
}

UsbInterfaceDescriptor UnpackInterfaceDescriptors(SmartBuffer& buf) {
  UsbInterfaceDescriptor result;
  CHECK(buf.size() >= sizeof(result));
  memcpy(&result, buf.data(), sizeof(result));
  buf.Erase(0, sizeof(result));

  result.bLength = ntohl(result.bLength);
  result.bDescriptorType = ntohl(result.bDescriptorType);
  result.bInterfaceNumber = ntohl(result.bInterfaceNumber);
  result.bAlternateSetting = ntohl(result.bAlternateSetting);
  result.bNumEndpoints = ntohl(result.bNumEndpoints);
  result.bInterfaceClass = ntohl(result.bInterfaceClass);
  result.bInterfaceSubClass = ntohl(result.bInterfaceSubClass);
  result.bInterfaceProtocol = ntohl(result.bInterfaceProtocol);
  result.iInterface = ntohl(result.iInterface);
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
