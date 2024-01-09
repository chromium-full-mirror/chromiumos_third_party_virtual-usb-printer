// Copyright 2020 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USB_DEVICE_SCANNER_SCAN_XML_UTIL_H_
#define VIRTUAL_USB_DEVICE_USB_DEVICE_SCANNER_SCAN_XML_UTIL_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "escl_manager.h"

// Returns a serialized eSCL ScannerCapabilities XML representation of |caps|.
// For fields that are not provided by |caps|, sensible default values are
// chosen.
std::vector<uint8_t> ScannerCapabilitiesAsXml(const ScannerCapabilities& caps);

// Returns a serialized eSCL ScannerStatus XML representation of |status|.
std::vector<uint8_t> ScannerStatusAsXml(const ScannerStatus& status);

// Attempts to parse a ScanSettings object from its xml representation, |xml|.
std::optional<ScanSettings> ScanSettingsFromXml(
    const std::vector<uint8_t>& xml);

#endif  // VIRTUAL_USB_DEVICE_USB_DEVICE_SCANNER_SCAN_XML_UTIL_H_
