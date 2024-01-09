// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scan_jpeg_util.h"

#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <base/check.h>
#include <base/check_op.h>
#include <base/logging.h>
#include <base/notreached.h>

#include <jerror.h>
#include <jpeglib.h>

namespace {

// All units should be escl:ThreeHundredthsOfInches. Dividing by 300 and
// multiplying by the resolution gives us the pixel count.
constexpr int kUnitsToPixelsDivisor = 300;

// Each quadrant of an image.
enum class ImageQuadrant { kTopLeft, kTopRight, kBottomLeft, kBottomRight };

// Populates `top_template` and `bottom_template` with pixel values appropriate
// to `color_mode` as follows:
//
// Black and white scans: the first half of `top_template` will be black and the
// second half will be white. The first half of `bottom_template` will be white
// and the second half will be black.
//
// Grayscale scans: the first half of `top_template` will be black and the
// second half will be dark gray. The first half of `bottom_template` will be
// light gray and the second half will be white.
//
// RGB scans: the first half of `top_template` will be blue and the second half
// will be green. The first half of `bottom_template` will be red and the second
// half will be yellow.
void SetRows(const ColorMode color_mode,
             const int width,
             JSAMPROW top_template,
             JSAMPROW bottom_template) {
  std::map<ImageQuadrant, std::vector<uint8_t>> quadrants;
  switch (color_mode) {
    case kBlackAndWhite:
      // Top left: black
      quadrants[ImageQuadrant::kTopLeft] = {0};
      // Top right: white
      quadrants[ImageQuadrant::kTopRight] = {255};
      // Bottom left: white
      quadrants[ImageQuadrant::kBottomLeft] = {255};
      // Bottom right: black
      quadrants[ImageQuadrant::kBottomRight] = {0};
      break;
    case kGrayscale:
      // Top left: black
      quadrants[ImageQuadrant::kTopLeft] = {0};
      // Top right: dark gray
      quadrants[ImageQuadrant::kTopRight] = {63};
      // Bottom left: light gray
      quadrants[ImageQuadrant::kBottomLeft] = {127};
      // Bottom right: white
      quadrants[ImageQuadrant::kBottomRight] = {255};
      break;
    case kRGB:
      // Top left: blue
      quadrants[ImageQuadrant::kTopLeft] = {0, 0, 255};
      // Top right: green
      quadrants[ImageQuadrant::kTopRight] = {0, 255, 0};
      // Bottom left: red
      quadrants[ImageQuadrant::kBottomLeft] = {255, 0, 0};
      // Bottom right: yellow
      quadrants[ImageQuadrant::kBottomRight] = {255, 150, 0};
      break;
    default:
      NOTREACHED();
      break;
  }

  int quadrant_width = width / 2;
  int quadrant_index = 0;

  // If we're using RBG, we can't stop in the middle of a pixel otherwise
  // all further samples will be shifted. This might mean we miss a single
  // pixel at the end of the row, but having one white pixel there is fine.
  if (color_mode == kRGB)
    quadrant_width -= quadrant_width % 3;

  for (int i = 0; i < quadrant_width; i++) {
    top_template[i] = quadrants[ImageQuadrant::kTopLeft][quadrant_index];
    top_template[i + quadrant_width] =
        quadrants[ImageQuadrant::kTopRight][quadrant_index];
    bottom_template[i] = quadrants[ImageQuadrant::kBottomLeft][quadrant_index];
    bottom_template[i + quadrant_width] =
        quadrants[ImageQuadrant::kBottomRight][quadrant_index];

    // Only RGB images have multiple components per pixel.
    if (color_mode == kRGB)
      quadrant_index = (quadrant_index + 1) % 3;
  }

  // If the width was odd, then we need to fill in one last sample.
  if (width % 2) {
    top_template[width - 1] =
        quadrants[ImageQuadrant::kTopLeft][quadrant_index];
    top_template[width - 1] =
        quadrants[ImageQuadrant::kTopRight][quadrant_index];
    bottom_template[width - 1] =
        quadrants[ImageQuadrant::kBottomLeft][quadrant_index];
    bottom_template[width - 1] =
        quadrants[ImageQuadrant::kBottomRight][quadrant_index];
  }
}

// Set values used by libjpeg: height and width of the image in pixels, color
// space and number of samples per pixel. Returns false if either the units or
// color_mode are unrecognized.
bool SetJpegCompressFields(const ScanSettings& settings,
                           jpeg_compress_struct* cinfo) {
  DCHECK(cinfo);
  DCHECK_EQ(settings.document_format, "image/jpeg");
  DCHECK_EQ(settings.regions.size(), 1);

  if (settings.regions[0].units != "escl:ThreeHundredthsOfInches") {
    LOG(ERROR) << "Unexpected units: " << settings.regions[0].units;
    return false;
  }

  cinfo->image_height = settings.regions[0].height * settings.y_resolution /
                        kUnitsToPixelsDivisor;
  cinfo->image_width =
      settings.regions[0].width * settings.x_resolution / kUnitsToPixelsDivisor;
  switch (settings.color_mode) {
    case kBlackAndWhite:  // FALLTHROUGH
    case kGrayscale:
      cinfo->input_components = 1;
      cinfo->in_color_space = JCS_GRAYSCALE;
      break;
    case kRGB:
      cinfo->input_components = 3;
      cinfo->in_color_space = JCS_RGB;
      break;
    default:
      LOG(ERROR) << "Unrecognized ColorMode: " << settings.color_mode;
      return false;
  }

  return true;
}

}  // namespace

std::optional<std::vector<uint8_t>> GenerateJpegFromScanSettings(
    const ScanSettings& settings) {
  // Allocate and initialize libjpeg objects.
  jpeg_compress_struct cinfo;
  jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);

  // Holds encoded size after JPEG conversion.
  unsigned long jpeg_size = 0;  // NOLINT(runtime/int) - size varies by device.
  // Holds JPEG image. NULL indicates that libjpeg will allocate the memory for
  // us, which we need to free when finished.
  unsigned char* jpeg_buffer = NULL;
  jpeg_mem_dest(&cinfo, &jpeg_buffer, &jpeg_size);

  if (!SetJpegCompressFields(settings, &cinfo))
    return std::nullopt;

  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, 100, TRUE);
  jpeg_start_compress(&cinfo, TRUE);

  // Create the rows used to generate the image.
  int num_samples = cinfo.image_width * cinfo.input_components;
  std::vector<uint8_t> top_template(num_samples);
  std::vector<uint8_t> bottom_template(num_samples);
  SetRows(settings.color_mode, num_samples, top_template.data(),
          bottom_template.data());

  // Write each row sequentially.
  JSAMPROW row_pointer[1];
  // First, the top template:
  row_pointer[0] = top_template.data();
  for (int i = 0; i < cinfo.image_height / 2; i++)
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  // Next, the bottom template:
  row_pointer[0] = bottom_template.data();
  for (int i = cinfo.image_height / 2; i < cinfo.image_height; i++)
    jpeg_write_scanlines(&cinfo, row_pointer, 1);

  // Finish JPEG conversion.
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  std::vector<uint8_t> image;
  image.assign(jpeg_buffer, jpeg_buffer + jpeg_size);

  free(jpeg_buffer);

  return image;
}
