// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "op_commands.h"

#include <arpa/inet.h>

#include <cstring>

#include <base/logging.h>

#include "device_descriptors.h"
#include "smart_buffer.h"
#include "usbip_constants.h"

// These are constants used to describe the exported device. They are used to
// populate the OpRepDevice message used when responding to OpReqDevlist and
// OpReqImport requests.
const char kUsbPath[] = "/sys/devices/pci0000:00/0000:00:01.2/usb1/1-1";
const char kBusId[] = "1-1";
constexpr int kBusnum = 1;
constexpr int kDevnum = 2;
constexpr int kSpeed = 3;  // Represents a high-speed USB device.
constexpr uint16_t kUsbipVersion = 0x0111;  // usbip version in BCD.

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
  snprintf(device->usbPath, sizeof(device->usbPath), "%s", kUsbPath);
  memset(device->busID, 0, sizeof(device->busID));
  snprintf(device->busID, sizeof(device->busID), "%s", kBusId);

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

void SetOpRepDevlistInterfaces(
    const std::vector<UsbInterfaceDescriptor>& interfaces,
    OpRepDevlistInterface** rep_interfaces) {
  // TODO(daviev): Change this to use a smart pointer at some point.
  *rep_interfaces = reinterpret_cast<OpRepDevlistInterface*>(
      malloc(interfaces.size() * sizeof(OpRepDevlistInterface)));
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

void SetOpRepDevInfo(const UsbDeviceDescriptor& device,
                     const UsbConfigurationDescriptor& config,
                     const std::vector<UsbInterfaceDescriptor>& interfaces,
                     OpRepDevInfo* info) {
  SetOpRepDevice(device, config, &info->device);
  SetOpRepDevlistInterfaces(interfaces, &info->interfaces);
}

void CreateOpRepImport(const UsbDeviceDescriptor& dev_dsc,
                       const UsbConfigurationDescriptor& config,
                       OpRepImport* rep) {
  SetOpHeader(OP_REP_IMPORT_CMD, 0, &rep->header);
  SetOpRepDevice(dev_dsc, config, &rep->device);
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

SmartBuffer PackOpRepDevInfo(OpRepDevInfo info) {
  SmartBuffer packed_device = PackOpRepDevice(info.device);
  size_t interfaces_size =
      sizeof(*info.interfaces) * info.device.bNumInterfaces;
  size_t buffer_size = sizeof(info.device) + interfaces_size;
  SmartBuffer packed_devinfo(buffer_size);
  packed_devinfo.Add(packed_device);
  packed_devinfo.Add(info.interfaces, interfaces_size);
  return packed_devinfo;
}

SmartBuffer PackOpRepDevlist(OpRepDevlist list) {
  SmartBuffer packed_header = PackOpRepDevlistHeader(list.header);
  LOG(INFO) << "Nofodevice: " << list.header.numExportedDevices;

  SmartBuffer packed_devlist;
  packed_devlist.Add(packed_header);
  for (auto& v : list.devices) {
    packed_devlist.Add(PackOpRepDevInfo(v));
  }
  return packed_devlist;
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

void UnpackDeviceData(DeviceData* data) {
  data->port = ntohs(data->port);
  data->device_type = ntohs(data->device_type);
  data->device_id = ntohs(data->device_id);
}

SmartBuffer PackDeviceData(DeviceData data) {
  data.port = htons(data.port);
  data.device_type = htons(data.device_type);
  data.device_id = htons(data.device_id);
  SmartBuffer packed_devdata(sizeof(data));
  packed_devdata.Add(&data, sizeof(data));
  return packed_devdata;
}

SmartBuffer PackBindRequest(BindRequest request) {
  SmartBuffer packed_bind;
  SmartBuffer packed_header = PackOpHeader(request.header);
  SmartBuffer packed_devdata = PackDeviceData(request.device_data);
  packed_bind.Add(packed_header);
  packed_bind.Add(packed_devdata);
  return packed_bind;
}
