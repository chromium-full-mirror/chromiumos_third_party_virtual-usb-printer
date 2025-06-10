// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "op_commands.h"

#include <arpa/inet.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include <base/check.h>
#include <base/logging.h>

#include "usbip_constants.h"
#include "virtual-usb-printer/common/smart_buffer.h"

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
    std::vector<OpRepDeviceInterface>* rep_interfaces) {
  rep_interfaces->resize(interfaces.size());
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

SmartBuffer PackOpRepDevInfo(OpRepDevInfo& info) {
  SmartBuffer packed_info = PackOpRepDevice(info.device);
  const size_t num = info.device.bNumInterfaces;
  for (size_t i = 0; i < num; ++i) {
    packed_info.Add(info.interfaces[i]);
  }

  return packed_info;
}

OpRepDevInfo UnpackOpRepDevInfo(SmartBuffer* buf) {
  OpRepDevInfo devinfo;
  CHECK(buf->size() >= sizeof(devinfo.device));
  memcpy(&devinfo.device, buf->data(), sizeof(devinfo.device));
  buf->Erase(0, sizeof(devinfo.device));
  UnpackOpRepDevice(&devinfo.device);

  devinfo.interfaces.resize(devinfo.device.bNumInterfaces);
  const size_t interface_size =
      sizeof(OpRepDeviceInterface) * devinfo.device.bNumInterfaces;
  memcpy(devinfo.interfaces.data(), buf->data(), interface_size);
  buf->Erase(0, interface_size);
  return devinfo;
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
  CHECK(buf->size() >= sizeof(OpRepDevlistHeader));
  memcpy(&list.header, buf->data(), sizeof(OpRepDevlistHeader));
  buf->Erase(0, sizeof(OpRepDevlistHeader));
  UnpackOpRepDevlistHeader(&list.header);

  CHECK(buf->size() >= list.header.numExportedDevices * sizeof(OpRepDevice));
  list.devices.resize(list.header.numExportedDevices);
  for (int i = 0; i < list.header.numExportedDevices; ++i) {
    list.devices[i] = UnpackOpRepDevInfo(buf);
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

SmartBuffer PackOpReqImport(OpReqImport import) {
  SmartBuffer packed_header = PackOpHeader(import.header);
  SmartBuffer packed_busID(sizeof(import.busID));
  packed_busID.Add(import.busID, sizeof(import.busID));
  SmartBuffer packed_import(sizeof(import));
  packed_import.Add(packed_header);
  packed_import.Add(packed_busID);
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

OpRepImport UnpackOpRepImport(SmartBuffer* buf) {
  OpRepImport result;
  CHECK(buf->size() >= sizeof(result));
  result.header = UnpackOpHeader(buf);
  memcpy(&result.device, buf->data(), sizeof(result.device));
  UnpackOpRepDevice(&result.device);
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

SmartBuffer PackBindOrUnbindRequest(BindOrUnbindRequest& request) {
  SmartBuffer buf;
  buf.Add(PackOpHeader(request.header));
  buf.Add(htons(request.port));
  return buf;
}
