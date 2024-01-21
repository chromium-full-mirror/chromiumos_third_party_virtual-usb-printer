// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "op_commands.h"

#include <arpa/inet.h>

#include <cstring>

#include <base/logging.h>

#include "virtual-usb-printer/common/smart_buffer.h"
#include "usbip_constants.h"

// These are constants used to describe the exported device. They are used to
// populate the OpRepDevice message used when responding to OpReqDevlist and
// OpReqImport requests.
constexpr int kBusnum = 1;
constexpr int kDevnum = 2;
constexpr int kSpeed = 3;  // Represents a high-speed USB device.

void SetOpHeader(uint16_t command, int status, OpHeader* header) {
  header->version = kUsbipVersion;
  header->command = command;
  header->status = status;
}

void SetOpRepDevlistHeader(uint16_t command,
                           int status,
                           int numExportedDevices,
                           OpRepDevlistHeader* devlist_header) {
  SetOpHeader(command, status, &devlist_header->header);
  devlist_header->numExportedDevices = numExportedDevices;
}

void SetOpRepDevice(const UsbDeviceDescriptor& dev_dsc,
                    const UsbConfigurationDescriptor& config,
                    OpRepDevice* device) {
  // Set constants.
  memset(device->usbPath, 0, sizeof(device->usbPath));
  memset(device->busID, 0, sizeof(device->busID));

  device->busnum = kBusnum;
  device->devnum = kDevnum;
  device->speed = kSpeed;

  // Set values using `dev_dsc`.
  device->idVendor = dev_dsc.idVendor;
  device->idProduct = dev_dsc.idProduct;
  device->bcdDevice = dev_dsc.bcdDevice;
  device->bDeviceClass = dev_dsc.bDeviceClass;
  device->bDeviceSubClass = dev_dsc.bDeviceSubClass;
  device->bDeviceProtocol = dev_dsc.bDeviceProtocol;
  device->bNumConfigurations = dev_dsc.bNumConfigurations;

  // Set values using `config`.
  device->bConfigurationValue = config.bConfigurationValue;
  device->bNumInterfaces = config.bNumInterfaces;
}

void SetOpRepDeviceInterfaces(
    const std::vector<UsbInterfaceDescriptor>& interfaces,
    OpRepDeviceInterface** rep_interfaces) {
  // TODO(daviev): Change this to use a smart pointer at some point.
  *rep_interfaces = reinterpret_cast<OpRepDeviceInterface*>(
      malloc(interfaces.size() * sizeof(OpRepDeviceInterface)));
  for (size_t i = 0; i < interfaces.size(); ++i) {
    const auto& interface = interfaces[i];
    (*rep_interfaces)[i].bInterfaceClass = interface.bInterfaceClass;
    (*rep_interfaces)[i].bInterfaceSubClass = interface.bInterfaceSubClass;
    (*rep_interfaces)[i].bInterfaceProtocol = interface.bInterfaceProtocol;
    (*rep_interfaces)[i].padding = 0;
  }
}

void CreateOpRepDevlistHeader(uint16_t numExportedDevices,
                              OpRepDevlist* message) {
  SetOpRepDevlistHeader(OP_REP_DEVLIST_CMD, 0, numExportedDevices,
                        &message->header);
}

void CreateOpRepDevInfo(const UsbDeviceDescriptor& device,
                        const UsbConfigurationDescriptor& config,
                        const std::vector<UsbInterfaceDescriptor>& interfaces,
                        OpRepDevInfo* info) {
  SetOpRepDevice(device, config, &info->device);
  SetOpRepDeviceInterfaces(interfaces, &info->interfaces);
}

void CreateOpRepImport(const OpRepDevInfo& devinfo, OpRepImport* rep) {
  SetOpHeader(OP_REP_IMPORT_CMD, 0, &rep->header);
  rep->device = devinfo.device;
}

SmartBuffer PackOpHeader(OpHeader header) {
  header.version = htons(header.version);
  header.command = htons(header.command);
  header.status = htonl(header.status);
  SmartBuffer packed_header(sizeof(header));
  packed_header.Add(&header, sizeof(header));
  return packed_header;
}

SmartBuffer PackOpRepDevice(OpRepDevice device) {
  device.busnum = htonl(device.busnum);
  device.devnum = htonl(device.devnum);
  device.speed = htonl(device.speed);
  device.idVendor = htons(device.idVendor);
  device.idProduct = htons(device.idProduct);
  device.bcdDevice = htons(device.bcdDevice);
  SmartBuffer packed_device(sizeof(device));
  packed_device.Add(&device, sizeof(device));
  return packed_device;
}

SmartBuffer PackOpRepDevlistHeader(OpRepDevlistHeader devlist_header) {
  SmartBuffer packed_op_header = PackOpHeader(devlist_header.header);
  devlist_header.numExportedDevices = htonl(devlist_header.numExportedDevices);
  SmartBuffer packed_header(sizeof(OpRepDevlistHeader));
  packed_header.Add(packed_op_header);
  packed_header.Add(devlist_header.numExportedDevices);
  return packed_header;
}

void UnpackOpRepDeviceInterface(OpRepDeviceInterface* interface) {
  interface->bInterfaceClass = ntohs(interface->bInterfaceClass);
  interface->bInterfaceSubClass = ntohs(interface->bInterfaceSubClass);
  interface->bInterfaceProtocol = ntohs(interface->bInterfaceProtocol);
  interface->padding = ntohs(interface->padding);
}

OpRepDeviceInterface UnpackOpRepDeviceInterface(SmartBuffer* buf) {
  OpRepDeviceInterface interface;
  CHECK(buf->size() >= sizeof(OpRepDeviceInterface));
  memcpy(&interface, buf->data(), sizeof(interface));
  buf->Erase(0, sizeof(interface));

  UnpackOpRepDeviceInterface(&interface);
  return interface;
}

SmartBuffer PackOpRepDeviceInterface(OpRepDeviceInterface interface) {
  SmartBuffer packed_buf;
  interface.bInterfaceClass = htons(interface.bInterfaceClass);
  interface.bInterfaceSubClass = htons(interface.bInterfaceSubClass);
  interface.bInterfaceProtocol = htons(interface.bInterfaceProtocol);
  interface.padding = ntohs(interface.padding);
  packed_buf.Add(interface);
  return packed_buf;
}

SmartBuffer PackOpRepDevInfo(OpRepDevInfo& info) {
  SmartBuffer packed_info = PackOpRepDevice(info.device);
  const size_t num = info.device.bNumInterfaces;
  for (size_t i = 0; i < num; ++i) {
    packed_info.Add(PackOpRepDeviceInterface(info.interfaces[i]));
  }

  return packed_info;
}

OpRepDevInfo UnpackOpRepDevInfo(SmartBuffer* buf) {
  OpRepDevInfo devinfo;
  CHECK(buf->size() >= sizeof(devinfo.device));
  memcpy(&devinfo.device, buf->data(), sizeof(devinfo.device));
  buf->Erase(0, sizeof(devinfo.device));
  UnpackOpRepDevice(&devinfo.device);

  const size_t interface_size =
      sizeof(OpRepDeviceInterface) * devinfo.device.bNumInterfaces;

  devinfo.interfaces =
      reinterpret_cast<OpRepDeviceInterface*>(malloc(interface_size));
  memcpy(devinfo.interfaces, buf->data(), interface_size);
  const size_t num = devinfo.device.bNumInterfaces;
  for (size_t i = 0; i < num; ++i) {
    UnpackOpRepDeviceInterface(devinfo.interfaces + i);
  }
  buf->Erase(0, interface_size);
  return devinfo;
}

void UnpackOpRepDevInfo(OpRepDevInfo* device_info) {
  UnpackOpRepDevice(&device_info->device);
  const size_t num = device_info->device.bNumInterfaces;
  for (size_t i = 0; i < num; ++i) {
    UnpackOpRepDeviceInterface(&device_info->interfaces[i]);
  }
}

SmartBuffer PackOpRepDevlist(OpRepDevlist list) {
  SmartBuffer packed_header = PackOpRepDevlistHeader(list.header);
  SmartBuffer packed_devlist;
  packed_devlist.Add(packed_header);
  for (auto& v : list.devices) {
    packed_devlist.Add(PackOpRepDevInfo(v));
  }
  return packed_devlist;
}

void UnpackOpRepDevlistHeader(OpRepDevlistHeader* header) {
  UnpackOpHeader(&header->header);
  header->numExportedDevices = ntohl(header->numExportedDevices);
}

OpRepDevlistHeader UnpackOpRepDevlistHeader(SmartBuffer* buf) {
  OpRepDevlistHeader header;
  CHECK(buf->size() >= sizeof(header));
  memcpy(&header, buf->data(), sizeof(header));
  buf->Erase(0, sizeof(header));
  UnpackOpRepDevlistHeader(&header);
  return header;
}

OpRepDevlist UnpackOpRepDevlist(SmartBuffer* buf) {
  OpRepDevlist list;
  CHECK(buf->size() >= sizeof(list));
  memcpy(&list, buf->data(), sizeof(list));
  buf->Erase(0, sizeof(list));
  UnpackOpRepDevlistHeader(&list.header);
  for (int i = 0; i < list.header.numExportedDevices; ++i) {
    UnpackOpRepDevInfo(&list.devices[i]);
  }
  return list;
}

SmartBuffer PackOpRepImport(OpRepImport import) {
  SmartBuffer packed_header = PackOpHeader(import.header);
  SmartBuffer packed_device = PackOpRepDevice(import.device);
  SmartBuffer packed_import(sizeof(import));
  packed_import.Add(packed_header);
  packed_import.Add(packed_device);
  return packed_import;
}

void UnpackOpHeader(OpHeader* header) {
  header->version = ntohs(header->version);
  header->command = ntohs(header->command);
  header->status = ntohl(header->status);
}

OpHeader UnpackOpHeader(SmartBuffer* buf) {
  OpHeader result;
  CHECK(buf->size() >= sizeof(result));
  memcpy(&result, buf->data(), sizeof(result));
  buf->Erase(0, sizeof(result));
  result.version = ntohs(result.version);
  result.command = ntohs(result.command);
  result.status = ntohl(result.status);
  return result;
}

OpRepDevice UnpackOpRepDevice(SmartBuffer* buf) {
  OpRepDevice device;
  CHECK(buf->size() >= sizeof(device));
  memcpy(&device, buf->data(), sizeof(device));
  buf->Erase(0, sizeof(device));
  device.busnum = ntohl(device.busnum);
  device.devnum = ntohl(device.devnum);
  device.speed = ntohl(device.speed);
  device.idVendor = ntohs(device.idVendor);
  device.idProduct = ntohs(device.idProduct);
  device.bcdDevice = ntohs(device.bcdDevice);
  return device;
}

void UnpackOpRepDevice(OpRepDevice* device) {
  device->busnum = ntohl(device->busnum);
  device->devnum = ntohl(device->devnum);
  device->speed = ntohl(device->speed);
  device->idVendor = ntohs(device->idVendor);
  device->idProduct = ntohs(device->idProduct);
  device->bcdDevice = ntohs(device->bcdDevice);
}

SmartBuffer PackBindRequest(BindRequest& request) {
  SmartBuffer buf;
  buf.Add(PackOpHeader(request.header));
  buf.Add(htons(request.port));
  buf.Add(PackOpRepDevInfo(request.device_desc));
  return buf;
}
