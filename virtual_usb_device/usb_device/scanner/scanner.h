// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USB_DEVICE_SCANNER_SCANNER_H_
#define VIRTUAL_USB_DEVICE_USB_DEVICE_SCANNER_SCANNER_H_

#include <memory>
#include <optional>
#include <string>

#include <base/files/file_path.h>
#include <base/values.h>

#include "escl_manager.h"
#include "virtual-usb-printer/common/http_util.h"
#include "virtual-usb-printer/common/smart_buffer.h"

// This class represent escl scanner and provides all functionality
// related to scanning.
class Scanner {
 public:
  // Creates `Scanner` object by reading JSON config file which provides escl
  // capabilites configuration. `logdir` is path where logs are stored during
  // operation.
  static std::unique_ptr<Scanner> Create(const std::string& capabilities_path,
                                         const base::FilePath log_dir);
  // Create scanner with no caps which results in having default EsclManager.
  Scanner();
  // Create Scanner object from escl capabilities and logging directory.
  Scanner(const ScannerCapabilities caps, const base::FilePath& logdir);
  ~Scanner() = default;

  // Generates an HTTP response for the given HttpRequest and request body.
  // If |request| is not a valid eSCL request (for example, invalid endpoint
  // or request method), the response will be an error response.
  HttpResponse HandleEsclRequest(const HttpRequest& request,
                                 const SmartBuffer& request_body);
  // Create ScannerCapabilities from the config dictionary.
  static std::optional<ScannerCapabilities> CreateScannerCapabilitiesFromConfig(
      const base::DictValue& config);

 private:
  // Attempts to initialize an ScannerCapabilities.
  // Parses |capabilities_path| into a JSON object in order to do so, returning
  // nullopt if that parsing fails.
  static std::optional<ScannerCapabilities> InitializeEsclCaps(
      const std::string& capabilities_path);

  std::unique_ptr<EsclManager> escl_manager_;
};

#endif  // VIRTUAL_USB_DEVICE_USB_DEVICE_SCANNER_SCANNER_H_
