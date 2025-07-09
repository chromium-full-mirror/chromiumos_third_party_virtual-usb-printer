// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scan_jpeg_util.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include <gtest/gtest.h>
#include <jerror.h>
#include <jpeglib.h>

#include "escl_manager.h"

#include <base/check.h>
#include <base/check_op.h>
#include <base/notreached.h>

namespace {

// Since libjpeg is not lossless, individual pixels of the decoded bitmap don't
// always match the exact colors they should. We allow pixels which are close to
// the expected value. This threshold value indicates how far from the expected
// value we allow.
constexpr int kAllowableThreshold = 1;

// Samples which represent a single pixel of a certain color.
constexpr uint8_t kBlueSample[] = {0, 0, 255};
constexpr uint8_t kGreenSample[] = {0, 255, 0};
constexpr uint8_t kRedSample[] = {255, 0, 0};
constexpr uint8_t kYellowSample[] = {255, 150, 0};
constexpr uint8_t kBlackSample = 0;
constexpr uint8_t kDarkGraySample = 63;
constexpr uint8_t kLightGraySample = 127;
constexpr uint8_t kWhiteSample = 255;

// Enumeration of the expected pixel values.
enum class Pixels {
  kUnknown,
  kBlue,
  kGreen,
  kRed,
  kYellow,
  kBlack,
  kDarkGray,
  kLightGray,
  kWhite,
};

// Checks to see if `sample` is within `kThreshold` of `color_sample`.
bool IsWithinThreshold(const uint8_t sample, const uint8_t color_sample) {
  return std::abs(static_cast<int>(sample) - static_cast<int>(color_sample)) <=
         kAllowableThreshold;
}

// Checks to see if each of `sample`'s three components are within `kThreshold`
// of `color_sample`'s three components.
bool IsWithinRgbThreshold(const std::vector<uint8_t>& sample,
                          const uint8_t* color_sample) {
  DCHECK(color_sample);
  DCHECK_EQ(sample.size(), 3);

  return IsWithinThreshold(sample[0], color_sample[0]) &&
         IsWithinThreshold(sample[1], color_sample[1]) &&
         IsWithinThreshold(sample[2], color_sample[2]);
}

// Filters `sample` to see if it matches one of the expected grayscale pixels.
// If `sample` is far from any of the expected pixels, Pixels::kUnknown is
// returned.
Pixels GetGrayscalePixelFromSample(uint8_t sample) {
  if (IsWithinThreshold(sample, kBlackSample)) {
    return Pixels::kBlack;
  }

  if (IsWithinThreshold(sample, kDarkGraySample)) {
    return Pixels::kDarkGray;
  }

  if (IsWithinThreshold(sample, kLightGraySample)) {
    return Pixels::kLightGray;
  }

  if (IsWithinThreshold(sample, kWhiteSample)) {
    return Pixels::kWhite;
  }

  return Pixels::kUnknown;
}

// Like GetGrayscalePixelFromSample above, but for RGB pixels.
Pixels GetRgbPixelFromSample(std::vector<uint8_t> sample) {
  if (IsWithinRgbThreshold(sample, kBlueSample)) {
    return Pixels::kBlue;
  }

  if (IsWithinRgbThreshold(sample, kGreenSample)) {
    return Pixels::kGreen;
  }

  if (IsWithinRgbThreshold(sample, kRedSample)) {
    return Pixels::kRed;
  }

  if (IsWithinRgbThreshold(sample, kYellowSample)) {
    return Pixels::kYellow;
  }

  return Pixels::kUnknown;
}

// Constructs a ScanSettings proto with the given `color_mode` and `dpi`, and a
// single scan region with the given `height` and `width`.
ScanSettings ConstructScanSettings(const ColorMode color_mode,
                                   const int dpi,
                                   const int height,
                                   const int width) {
  ScanRegion region;
  region.units = "escl:ThreeHundredthsOfInches";
  region.height = height;
  region.width = width;

  ScanSettings settings;
  settings.document_format = "image/jpeg";
  settings.color_mode = color_mode;
  settings.x_resolution = dpi;
  settings.y_resolution = dpi;
  settings.regions.push_back(region);

  return settings;
}

// Decompresses `jpeg` into a rectangular bitmap, where bitmap[i][j] is the j-th
// component of the i-th row of the bitmap.
std::vector<std::vector<uint8_t>> GetBitmapFromJpeg(
    const std::vector<uint8_t>& jpeg) {
  jpeg_decompress_struct cinfo;
  jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, jpeg.data(), jpeg.size());
  jpeg_read_header(&cinfo, true);
  jpeg_start_decompress(&cinfo);

  std::vector<std::vector<uint8_t>> bitmap;
  std::vector<uint8_t> row(cinfo.output_width * cinfo.output_components);
  JSAMPROW buffer[1];
  buffer[0] = row.data();

  while (cinfo.output_scanline < cinfo.output_height) {
    jpeg_read_scanlines(&cinfo, buffer, 1);
    bitmap.push_back(row);
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  return bitmap;
}

// Splits `row` into a vector of Pixels, which are easier to reason about.
std::vector<Pixels> GetPixelsFromRow(const ColorMode color_mode,
                                     const std::vector<uint8_t>& row) {
  std::vector<Pixels> pixels;

  if (color_mode == kRGB) {
    for (int i = 0; i < row.size() - 2; i += 3) {
      std::vector<uint8_t> sample = {row[i], row[i + 1], row[i + 2]};
      pixels.push_back(GetRgbPixelFromSample(sample));
    }
  } else {
    for (const auto& sample : row)
      pixels.push_back(GetGrayscalePixelFromSample(sample));
  }

  return pixels;
}

// Helper function used by MatchesExpectedTopTemplate and
// MatchesExpectedBottomTemplate.
bool MatchesTemplateColors(const ColorMode color_mode,
                           std::vector<uint8_t> row_template,
                           const Pixels first_expected_pixel,
                           const Pixels second_expected_pixel) {
  std::vector<Pixels> actual_pixels =
      GetPixelsFromRow(color_mode, row_template);

  int num_matching_pixels = 0;

  Pixels expected_pixel;
  for (int i = 0; i < actual_pixels.size(); i++) {
    expected_pixel = i < actual_pixels.size() / 2 ? first_expected_pixel
                                                  : second_expected_pixel;
    if (actual_pixels[i] == expected_pixel)
      num_matching_pixels++;
  }

  return static_cast<double>(num_matching_pixels) /
             static_cast<double>(actual_pixels.size()) >
         0.99;
}

// Returns true iff `top_template` matches the expected top template for
// `color_mode` for at least 99% of its pixels. See the comment for
// GenerateJpegFromScanSettings in jpeg_util.h for details on the expected
// templates.
bool MatchesExpectedTopTemplate(const ColorMode color_mode,
                                const std::vector<uint8_t>& top_template) {
  switch (color_mode) {
    case kBlackAndWhite:
      return MatchesTemplateColors(color_mode, top_template, Pixels::kBlack,
                                   Pixels::kWhite);
    case kGrayscale:
      return MatchesTemplateColors(color_mode, top_template, Pixels::kBlack,
                                   Pixels::kDarkGray);
    case kRGB:
      return MatchesTemplateColors(color_mode, top_template, Pixels::kBlue,
                                   Pixels::kGreen);
    default:
      NOTREACHED();
  }
}

// Returns true iff `bottom_template` matches the expected bottom template for
// `color_mode` for at least 99% of its pixels. See the comment for
// GenerateJpegFromScanSettings in jpeg_util.h for details on the expected
// templates.
bool MatchesExpectedBottomTemplate(
    const ColorMode color_mode, const std::vector<uint8_t>& bottom_template) {
  switch (color_mode) {
    case kBlackAndWhite:
      return MatchesTemplateColors(color_mode, bottom_template, Pixels::kWhite,
                                   Pixels::kBlack);
    case kGrayscale:
      return MatchesTemplateColors(color_mode, bottom_template,
                                   Pixels::kLightGray, Pixels::kWhite);
    case kRGB:
      return MatchesTemplateColors(color_mode, bottom_template, Pixels::kRed,
                                   Pixels::kYellow);
    default:
      NOTREACHED();
  }
}

}  // namespace

class JpegUtilsTest : public testing::Test,
                      public testing::WithParamInterface<ScanSettings> {
 protected:
  JpegUtilsTest() = default;
  JpegUtilsTest(const JpegUtilsTest&) = delete;
  JpegUtilsTest& operator=(const JpegUtilsTest&) = delete;

  ScanSettings settings() const { return GetParam(); }
};

// Test that the generated JPEG matches the expected image. Note that libjpeg is
// not lossless, so we cannot compare 100% of the pixels. Instead, we verify
// that the image has the correct number of pixels, and that 99% or more of each
// pixel in 99% or more of each row is approximately the correct value.
TEST_P(JpegUtilsTest, GenerateJpegFromScanSettings) {
  std::optional<std::vector<uint8_t>> jpeg =
      GenerateJpegFromScanSettings(settings());
  ASSERT_TRUE(jpeg.has_value());
  std::vector<std::vector<uint8_t>> bitmap = GetBitmapFromJpeg(jpeg.value());

  // Verify that we have the expected number of rows.
  ASSERT_EQ(settings().regions.size(), 1);
  EXPECT_EQ(bitmap.size(),
            settings().regions[0].height * settings().y_resolution / 300);

  int num_matching_rows = 0;
  for (int i = 0; i < bitmap.size() / 2; i++) {
    // Verify that the row has the correct number of samples.
    if (settings().color_mode == kRGB) {
      EXPECT_EQ(bitmap[i].size(), settings().regions[0].width *
                                      settings().x_resolution / 300 * 3);
    } else {
      EXPECT_EQ(bitmap[i].size(),
                settings().regions[0].width * settings().x_resolution / 300);
    }

    // Verify that the row contains the correct samples.
    if (MatchesExpectedTopTemplate(settings().color_mode, bitmap[i]))
      num_matching_rows++;

    if (MatchesExpectedBottomTemplate(settings().color_mode,
                                      bitmap[i + bitmap.size() / 2])) {
      num_matching_rows++;
    }
  }

  EXPECT_GT(static_cast<double>(num_matching_rows) /
                static_cast<double>(bitmap.size()),
            0.99);
}

// We won't test every possible combination, but we will make sure to hit every
// color mode, resolution and standard size at least once.
INSTANTIATE_TEST_SUITE_P(
    All,
    JpegUtilsTest,
    testing::Values(
        ConstructScanSettings(
            kBlackAndWhite, 75, 3300, 2550),  // B&W, 75 DPI, Letter
        ConstructScanSettings(
            kGrayscale, 100, 3507, 2480),  // Grayscale, 100 DPI, A4
        ConstructScanSettings(
            kRGB, 150, 4200, 2551),  // Color, 150 DPI, Fit to scan area
        ConstructScanSettings(
            kBlackAndWhite, 200, 4200, 2551),  // B&W, 200 DPI, Fit to scan area
        ConstructScanSettings(
            kGrayscale, 300, 3300, 2550),  // Grayscale, 300 DPI, Letter
        ConstructScanSettings(kRGB, 600, 3507, 2480)));  // Color, 600 DPI, A4
