// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USB_DEVICE_SCANNER_SCAN_JPEG_UTIL_H_
#define VIRTUAL_USB_DEVICE_USB_DEVICE_SCANNER_SCAN_JPEG_UTIL_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "escl_manager.h"

// Generates a JPEG image from `settings`. Returns std::nullopt if `settings`
// cannot be parsed, which implies the scan region's units were unknown. The
// generated image will be four equal quadrants, whose values depend on the
// request color mode:
//
// Black and white:
// Black - White
// White - Black
//
// Grayscale:
// Black - Dark Gray
// Light Gray - White
//
// RGB:
// Blue - Green
// Red - Yellow
std::optional<std::vector<uint8_t>> GenerateJpegFromScanSettings(
    const ScanSettings& settings);

#endif  // VIRTUAL_USB_DEVICE_USB_DEVICE_SCANNER_SCAN_JPEG_UTIL_H_
