// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JPEG_UTIL_H_
#define JPEG_UTIL_H_

#include <cstdint>
#include <vector>

#include <base/optional.h>

#include "escl_manager.h"

// Generates a JPEG image from `settings`. Returns base::nullopt if `settings`
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
base::Optional<std::vector<uint8_t>> GenerateJpegFromScanSettings(
    const ScanSettings& settings);

#endif  // JPEG_UTIL_H_
