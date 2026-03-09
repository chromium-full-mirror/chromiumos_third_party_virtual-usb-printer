// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scanner.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <base/check.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/values.h>

#include "escl_manager.h"
#include "virtual-usb-printer/common/http_util.h"
#include "virtual-usb-printer/common/smart_buffer.h"

namespace {

std::optional<std::vector<std::string>> ExtractStringList(
    const base::DictValue& root, const std::string& config_name) {
  const base::ListValue* value = root.FindList(config_name);
  if (!value) {
    LOG(ERROR) << "Config is missing " << config_name << " settings";
    return std::nullopt;
  }
  std::vector<std::string> config_list;
  for (const base::Value& v : *value) {
    if (!v.is_string()) {
      LOG(ERROR) << config_name << " value expected string, not " << v.type();
      return std::nullopt;
    }
    config_list.push_back(v.GetString());
  }
  return config_list;
}

std::optional<std::vector<int>> ExtractIntList(const base::DictValue& root,
                                               const std::string& config_name) {
  const base::ListValue* value = root.FindList(config_name);
  if (!value) {
    LOG(ERROR) << "Config is missing " << config_name << " settings";
    return std::nullopt;
  }
  std::vector<int> config_list;
  for (const base::Value& v : *value) {
    if (!v.is_int()) {
      LOG(ERROR) << config_name << " value expected int, not " << v.type();
      return std::nullopt;
    }
    config_list.push_back(v.GetInt());
  }
  return config_list;
}

std::optional<SourceCapabilities> CreateSourceCapabilitiesFromConfig(
    const base::DictValue& config) {
  SourceCapabilities result;
  result.max_width = config.FindInt("MaxWidth");
  result.max_height = config.FindInt("MaxHeight");

  std::optional<std::vector<std::string>> color_modes =
      ExtractStringList(config, "ColorModes");
  if (!color_modes) {
    LOG(ERROR) << "Could not find valid ColorModes config";
    return std::nullopt;
  }
  result.color_modes = color_modes.value();

  std::optional<std::vector<std::string>> formats =
      ExtractStringList(config, "DocumentFormats");
  if (!formats) {
    LOG(ERROR) << "Could not find valid DocumentFormats config";
    return std::nullopt;
  }
  result.formats = formats.value();

  std::optional<std::vector<int>> resolutions =
      ExtractIntList(config, "Resolutions");
  if (!resolutions) {
    LOG(ERROR) << "Could not find valid Resolutions config";
    return std::nullopt;
  }
  result.resolutions = resolutions.value();

  const std::string* x_justification = config.FindString("XJustification");
  if (!x_justification)
    LOG(ERROR) << "Config is missing XJustification setting";
  result.x_justification =
      x_justification ? std::make_optional(*x_justification) : std::nullopt;

  return result;
}

}  // namespace

// static
std::optional<ScannerCapabilities> Scanner::CreateScannerCapabilitiesFromConfig(
    const base::DictValue& config) {
  ScannerCapabilities result;
  const std::string* make_and_model = config.FindString("MakeAndModel");
  if (!make_and_model) {
    LOG(ERROR) << "Config is missing MakeAndModel setting";
    return std::nullopt;
  }
  result.make_and_model = *make_and_model;

  const std::string* serial_number = config.FindString("SerialNumber");
  if (!serial_number) {
    LOG(ERROR) << "Config is missing SerialNumber setting";
    return std::nullopt;
  }
  result.serial_number = *serial_number;

  const base::DictValue* platen = config.FindDict("Platen");
  if (!platen) {
    LOG(ERROR) << "Config is missing Platen source capabilities";
    return std::nullopt;
  }

  std::optional<SourceCapabilities> platen_capabilities =
      CreateSourceCapabilitiesFromConfig(*platen);
  if (!platen_capabilities) {
    LOG(ERROR) << "Parsing Platen capabilities failed";
    return std::nullopt;
  }
  result.platen_capabilities = platen_capabilities.value();

  const base::DictValue* adf = config.FindDict("ADF");
  if (adf) {
    std::optional<SourceCapabilities> adf_capabilities =
        CreateSourceCapabilitiesFromConfig(*adf);
    if (!adf_capabilities) {
      LOG(ERROR) << "Parsing ADF capabilities failed";
      return std::nullopt;
    }
    result.adf_capabilities = adf_capabilities;
  }

  return result;
}

// static
std::unique_ptr<Scanner> Scanner::Create(const std::string& capabilities_path,
                                         const base::FilePath log_dir) {
  if (capabilities_path.empty()) {
    return std::make_unique<Scanner>();
  }
  std::optional<ScannerCapabilities> scanner_caps =
      InitializeEsclCaps(capabilities_path);
  if (!scanner_caps.has_value()) {
    return nullptr;
  }

  return std::make_unique<Scanner>(scanner_caps.value(), log_dir);
}

std::optional<ScannerCapabilities> Scanner::InitializeEsclCaps(
    const std::string& capabilities_path) {
  if (capabilities_path.empty()) {
    return std::nullopt;
  }

  std::string capabilities_string;
  if (!base::ReadFileToString(base::FilePath(capabilities_path),
                              &capabilities_string)) {
    LOG(ERROR) << "Failed to read " << capabilities_path;
    return std::nullopt;
  }

  std::optional<base::Value> capabilities_json = base::JSONReader::Read(
      capabilities_string, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!capabilities_json) {
    LOG(ERROR) << "Failed to parse capabilities as JSON";
    return std::nullopt;
  }

  if (!capabilities_json->is_dict()) {
    LOG(ERROR) << "Cannot initialize ScannerCapabilities from non-dict value";
    return std::nullopt;
  }

  std::optional<ScannerCapabilities> capabilities =
      CreateScannerCapabilitiesFromConfig(capabilities_json->GetDict());
  if (!capabilities) {
    LOG(ERROR) << "Failed to initialize ScannerCapabilities";
    return std::nullopt;
  }

  return capabilities;
}

Scanner::Scanner() : escl_manager_(new EsclManager()) {}

Scanner::Scanner(const ScannerCapabilities caps, const base::FilePath& logdir) {
  escl_manager_.reset(new EsclManager(caps, logdir));
}

HttpResponse Scanner::HandleEsclRequest(const HttpRequest& request,
                                        const SmartBuffer& request_body) {
  return escl_manager_->HandleEsclRequest(request, request_body);
}
