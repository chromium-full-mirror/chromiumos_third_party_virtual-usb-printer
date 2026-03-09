// Copyright 2020 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "escl_manager.h"

#include <optional>
#include <utility>
#include <algorithm>

#include <base/files/file_util.h>
#include <base/json/json_writer.h>
#include <base/strings/stringprintf.h>
#include <base/logging.h>
#include <base/rand_util.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/threading/platform_thread.h>

#include "scan_jpeg_util.h"
#include "scan_xml_util.h"

namespace {

// Generates a hyphenated UUID v4 (random).
// An example UUID looks like "0b2cdf31-edee-4246-a1ad-07bbe754856b"
std::string GenerateUUID() {
  // We only need 16 bytes of randomness, not 32, but using 32 makes the rest of
  // the code a little clearer.
  std::vector<uint8_t> bytes(32);
  base::RandBytes(bytes);

  std::string uuid;
  for (int i = 0; i < 32; i++) {
    if (i == 8 || i == 12 || i == 16 || i == 20)
      uuid += "-";

    uint8_t val = bytes[i] % 16;
    if (i == 12) {
      val = 4;
    } else if (i == 16) {
      val &= ~0x4;
      val |= 0x8;
    }

    if (0 <= val && val <= 9)
      uuid += '0' + val;
    else
      uuid += 'a' + (val - 10);
  }
  return uuid;
}

std::string JobStateAsString(JobState state) {
  switch (state) {
    case kCanceled:
      return "Canceled";
    case kCompleted:
      return "Completed";
    case kPending:
      return "Pending";
  }
}

std::string ColorModeAsString(ColorMode mode) {
  switch (mode) {
    case kBlackAndWhite:
      return "BlackAndWhite1";
    case kGrayscale:
      return "Grayscale8";
    case kRGB:
      return "RGB24";
  }
}

uint8_t GetTotalPagesForJob(const ScanSettings& settings) {
  // Two scan jobs consist of multiple pages:
  //   * ADF grayscale 300 dpi.
  //   * ADF color 100 dpi.
  if ((settings.input_source == "Feeder" && settings.color_mode == kGrayscale &&
       settings.x_resolution == 300 && settings.y_resolution == 300) ||
      (settings.input_source == "Feeder" && settings.color_mode == kRGB &&
       settings.x_resolution == 100 && settings.y_resolution == 100)) {
    return 2;
  }

  return 1;
}

std::optional<base::TimeDelta> GetDelayForJob(const ScanSettings& settings) {
  // The only scan job which sends a delayed response is a Platen scan which is
  // color and 100 dpi.
  if (settings.input_source == "Platen" && settings.color_mode == kRGB &&
      settings.x_resolution == 100 && settings.y_resolution == 100) {
    return base::Seconds(45);
  }

  return std::nullopt;
}

}  // namespace

EsclManager::EsclManager(ScannerCapabilities scanner_capabilities,
                         const base::FilePath& output_log_dir)
    : scanner_capabilities_(std::move(scanner_capabilities)),
      output_log_dir_(output_log_dir),
      log_counter_(1) {
  status_.idle = true;
  status_.adf_empty = false;
}

HttpResponse EsclManager::HandleEsclRequest(const HttpRequest& request,
                                            const SmartBuffer& request_body) {
  if (request.method == "GET" && request.uri == "/eSCL/ScannerCapabilities") {
    HttpResponse response;
    response.status = "200 OK";
    response.headers["Content-Type"] = "text/xml";
    response.body.Add(ScannerCapabilitiesAsXml(scanner_capabilities_));
    return response;
  } else if (request.method == "GET" && request.uri == "/eSCL/ScannerStatus") {
    HttpResponse response;
    response.status = "200 OK";
    response.headers["Content-Type"] = "text/xml";
    response.body.Add(ScannerStatusAsXml(status_));
    status_.adf_empty = false;
    return response;
  } else if (request.method == "POST" && request.uri == "/eSCL/ScanJobs") {
    return HandleCreateScanJob(request_body);
  } else if (request.method == "GET" &&
             base::StartsWith(request.uri, "/eSCL/ScanJobs/",
                              base::CompareCase::SENSITIVE)) {
    return HandleGetNextDocument(request.uri);
  } else if (request.method == "DELETE" &&
             base::StartsWith(request.uri, "/eSCL/ScanJobs/",
                              base::CompareCase::SENSITIVE)) {
    return HandleDeleteJob(request.uri);
  } else if (request.uri == "/eSCL/ScannerCapabilities" ||
             request.uri == "/eSCL/ScannerStatus" ||
             base::StartsWith(request.uri, "/eSCL/ScanJobs",
                              base::CompareCase::SENSITIVE)) {
    LOG(ERROR) << "Unexpected request method " << request.method
               << " for endpoint " << request.uri;
    HttpResponse response;
    response.status = "405 Method Not Allowed";
    return response;
  } else {
    LOG(ERROR) << "Unknown eSCL endpoint " << request.uri << " (method is "
               << request.method << ")";
    HttpResponse response;
    response.status = "404 Not Found";
    return response;
  }
}

// Generates an HTTP response for a POST request to the /eSCL/ScanJobs endpoint.
HttpResponse EsclManager::HandleCreateScanJob(const SmartBuffer& request_body) {
  HttpResponse response;

  std::optional<ScanSettings> settings_opt =
      ScanSettingsFromXml(request_body.contents());
  if (!settings_opt) {
    LOG(ERROR) << "Could not parse ScanSettings from request body";
    response.status = "415 Unsupported Media Type";
    return response;
  }
  ScanSettings settings = settings_opt.value();
  if (!LogSettings(settings)) {
    LOG(ERROR) << "Failed to log settings.";
  }

  SourceCapabilities caps;
  if (settings.input_source == "Platen") {
    caps = scanner_capabilities_.platen_capabilities;
  } else if (settings.input_source == "Feeder") {
    if (!scanner_capabilities_.adf_capabilities.has_value()) {
      LOG(ERROR) << "Requested ADF source but printer doesn't have an ADF";
      response.status = "409 Conflict";
      return response;
    }
    caps = scanner_capabilities_.adf_capabilities.value();
  } else {
    LOG(ERROR) << "Requested unknown source '" << settings.input_source << "'";
    response.status = "409 Conflict";
    return response;
  }

  if (!std::ranges::contains(caps.color_modes,
                             ColorModeAsString(settings.color_mode))) {
    LOG(ERROR) << "Requested unsupported color mode '"
               << ColorModeAsString(settings.color_mode) << "'";
    for (const auto& mode : caps.color_modes) {
      LOG(ERROR) << "modes: " << mode;
    }
    response.status = "409 Conflict";
    return response;
  }

  if (!std::ranges::contains(caps.formats, settings.document_format)) {
    LOG(ERROR) << "Requested unsupported document format '"
               << settings.document_format << "'";
    response.status = "409 Conflict";
    return response;
  }

  if (settings.x_resolution != settings.y_resolution) {
    LOG(ERROR) << "Scanner cannot support different resolutions in X and Y: "
               << settings.x_resolution << " " << settings.y_resolution;
    response.status = "409 Conflict";
    return response;
  }

  if (!std::ranges::contains(caps.resolutions, settings.x_resolution)) {
    LOG(ERROR) << "Requested unsupported resolution '" << settings.x_resolution
               << "'";
    response.status = "409 Conflict";
    return response;
  }

  if (settings.input_source == "Feeder" && settings.color_mode == kGrayscale &&
      settings.x_resolution == 600 && settings.y_resolution == 600) {
    status_.adf_empty = true;
    response.status = "500 Internal Server Error";
    return response;
  }

  std::string uuid = GenerateUUID();
  JobInfo job;
  job.created = base::TimeTicks::Now();
  job.state = kPending;
  job.settings = std::move(settings);
  job.num_remaining_pages = GetTotalPagesForJob(job.settings);
  job.delay = GetDelayForJob(job.settings);
  status_.jobs[uuid] = job;

  response.status = "201 Created";
  response.headers["Location"] = "/eSCL/ScanJobs/" + uuid;
  response.headers["Pragma"] = "no-cache";
  return response;
}

// Generates an HTTP response containing scan data for a previously created
// scan job.
// The URI should be formatted as:
//   "/eSCL/ScanJobs/0b2cdf31-edee-4246-a1ad-07bbe754856b/NextDocument"
HttpResponse EsclManager::HandleGetNextDocument(const std::string& uri) {
  HttpResponse response;
  std::vector<std::string> tokens =
      base::SplitString(uri, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() != 5 || tokens[4] != "NextDocument") {
    LOG(ERROR) << "Malformed GET ScanJobs request URI: " << uri;
    response.status = "405 Method Not Allowed";
    return response;
  }

  std::string uuid = tokens[3];
  if (status_.jobs.count(uuid) == 0) {
    LOG(ERROR) << "No job found with uuid: " << uuid;
    response.status = "404 Not Found";
    return response;
  }

  JobInfo info = status_.jobs[uuid];
  switch (info.state) {
    case kCanceled:
    case kCompleted:
      LOG(INFO) << "Not providing NextDocument for "
                << JobStateAsString(info.state) << " job.";
      response.status = "404 Not Found";
      break;
    case kPending: {
      status_.jobs[uuid].num_remaining_pages--;
      if (status_.jobs[uuid].num_remaining_pages == 0)
        status_.jobs[uuid].state = kCompleted;
      std::optional<std::vector<uint8_t>> image =
          GenerateJpegFromScanSettings(info.settings);
      if (!image.has_value()) {
        LOG(ERROR) << "Failed to generate image, sending error response.";
        response.status = "500 Internal Server Error";
      } else {
        response.status = "200 OK";
        response.headers["Content-Type"] = "image/jpeg";
        response.body.Add(image.value());

        // Unfortunately we don't have access to a callback to post a delayed
        // task which runs the callback, so the easiest way to delay the
        // response is just to sleep.
        if (info.delay.has_value())
          base::PlatformThread::Sleep(info.delay.value());
      }
      break;
    }
  }

  return response;
}

// Generates an HTTP response to a request to delete the ScanJob at |uri|.
HttpResponse EsclManager::HandleDeleteJob(const std::string& uri) {
  HttpResponse response;
  std::vector<std::string> tokens =
      base::SplitString(uri, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() != 4) {
    LOG(ERROR) << "Malformed DELETE ScanJobs request URI: " << uri;
    response.status = "405 Method Not Allowed";
    return response;
  }

  std::string uuid = tokens[3];
  size_t erased = status_.jobs.erase(uuid);
  if (erased == 1) {
    response.status = "200 OK";
  } else {
    response.status = "404 Not Found";
  }
  return response;
}

bool EsclManager::LogSettings(const ScanSettings& settings) {
  if (output_log_dir_.empty())
    return true;

  base::DictValue settings_dict = SettingsToDict(settings);
  std::string json_settings;
  if (!base::JSONWriter::Write(settings_dict, &json_settings)) {
    LOG(ERROR) << "Failed to write settings in json format.";
    return false;
  }

  base::FilePath json_file_path = output_log_dir_.Append(
      base::StringPrintf("%02d_createscanjob.json", log_counter_));
  if (!base::WriteFile(json_file_path, json_settings)) {
    LOG(ERROR) << "Failed to write settings to json file.";
    return false;
  }

  return true;
}

base::DictValue EsclManager::SettingsToDict(const ScanSettings& settings) {
  base::DictValue dict;
  dict.Set("DocumentFormat", settings.document_format);
  dict.Set("ColorMode", settings.color_mode);
  dict.Set("InputSource", settings.input_source);
  dict.Set("XResolution", settings.x_resolution);
  dict.Set("YResolution", settings.y_resolution);
  base::ListValue regions;
  for (const ScanRegion& r : settings.regions) {
    base::DictValue region_dict;
    region_dict.Set("Units", r.units);
    region_dict.Set("Height", r.height);
    region_dict.Set("Width", r.width);
    region_dict.Set("XOffset", r.x_offset);
    region_dict.Set("YOffset", r.y_offset);
    regions.Append(std::move(region_dict));
  }
  dict.Set("Regions", std::move(regions));

  return dict;
}
