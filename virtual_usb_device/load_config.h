// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_LOAD_CONFIG_H_
#define VIRTUAL_USB_DEVICE_LOAD_CONFIG_H_

#include <cstdint>
#include <string>
#include <vector>

#include <base/values.h>
#include <base/containers/flat_map.h>

#include "device_descriptors.h"
#include "usbip_constants.h"

// Extract the uint8_t value associated with `key` from `dict`.
uint8_t GetByteValue(const base::DictValue& dict, const std::string& key);

// Extract the uint16_t value associated with `key` from `dict`.
uint16_t GetWordValue(const base::DictValue& dict, const std::string& key);

// Extract the USB device descriptor from the given `printer` config JSON.
UsbDeviceDescriptor GetDeviceDescriptor(const base::DictValue& printer);

// Extract the USB configuration descriptor from the given `printer` config
// JSON.
UsbConfigurationDescriptor GetConfigurationDescriptor(
    const base::DictValue& printer);

// Extract the USB device qualifier descriptor from the given `printer` config
// JSON.
UsbDeviceQualifierDescriptor GetDeviceQualifierDescriptor(
    const base::DictValue& printer);

// Extract each of the USB interface descriptors from the given `printer` config
// JSON and return them in a vector.
std::vector<UsbInterfaceDescriptor> GetInterfaceDescriptors(
    const base::DictValue& printer);

// Extract the values from the given interface descriptor JSON `descriptor`.
UsbInterfaceDescriptor GetInterfaceDescriptor(
    const base::DictValue& descriptor);

// Extract the interface descriptors and their associated endpoint descriptors
// to construct a mapping from interface numbers to a collection of endpoint
// descriptors.
base::flat_map<uint8_t, std::vector<UsbEndpointDescriptor>>
GetEndpointDescriptorMap(const base::DictValue& printer);

// Extract the USB endpoint descriptor from the given `printer` config JSON.
UsbEndpointDescriptor GetEndpointDescriptor(const base::DictValue& printer);

// Converts `string` into a USB string descriptor stored in a vector of
// characters.
std::vector<char> ConvertStringToStringDescriptor(const std::string& str);

// Extract the string descriptors from the given `printer` config JSON. The
// `printer` JSON is expected to contain the key "language_descriptor" which
// represents the special language string descriptor. The following string
// descriptors are expected to be stored in a list associated with the key
// "string_descriptors".
std::vector<std::vector<char>> GetStringDescriptors(
    const base::DictValue& printer);

// Extracts the IEEE Device ID from the given `printer` config JSON.
std::vector<char> GetIEEEDeviceId(const base::DictValue& printer);

#endif  // VIRTUAL_USB_DEVICE_LOAD_CONFIG_H_
