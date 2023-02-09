// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/check.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/values.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "device_descriptors.h"
#include "ipp_manager.h"
#include "load_config.h"
#include "mock_printer/mock_printer.h"
#include "mock_printer/parse_textproto.h"
#include "mock_printer/proto/control_flow.pb.h"
#include "server.h"
#include "usb_printer.h"
#include "usbip.h"
#include "usbip_constants.h"
#include "value_util.h"

namespace {

constexpr char kUsage[] =
    "virtual_usb_printer\n"
    "    --descriptors_path=<path>\n"
    "    [--record_doc_path=<path>]\n"
    "    [--attributes_path=<path>]\n"
    "    [--scanner_capabilities_path=<path>]\n"
    "    [--mock_printer_script=<path>]\n"
    "    [--http_header_output_dir=<path>]\n"
    "    [--output_log_directory=<path>]";

// Attempts to initialize an EsclManager.
// Parses |capabilities_path| into a JSON object in order to do so, returning
// nullopt if that parsing fails.
std::optional<EsclManager> InitializeEsclManager(
    const std::string& capabilities_path, base::FilePath log_dir) {
  if (capabilities_path.empty()) {
    return EsclManager();
  }

  std::string capabilities_string;
  if (!base::ReadFileToString(base::FilePath(capabilities_path),
                              &capabilities_string)) {
    LOG(ERROR) << "Failed to read " << capabilities_path;
    return std::nullopt;
  }

  std::optional<base::Value> capabilities_json =
      base::JSONReader::Read(capabilities_string);
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

  return EsclManager(std::move(capabilities.value()), log_dir);
}

// Creates a `MockPrinter` using the `mocking::TestCase` textproto at
// `script_path`.
std::unique_ptr<mock_printer::MockPrinter> InitializeMockPrinter(
    base::StringPiece script_path) {
  CHECK(!script_path.empty()) << "BUG: expected nonempty `script_path`";

  std::string textproto;
  if (!base::ReadFileToString(base::FilePath(script_path), &textproto)) {
    LOG(ERROR) << "Failed to read textproto " << script_path;
    return nullptr;
  }

  std::optional<mocking::TestCase> script =
      mock_printer::ParseTestCase(textproto);
  if (!script.has_value()) {
    LOG(ERROR) << "Failed to parse `mocking::TestCase` from " << script_path;
    return nullptr;
  }

  return mock_printer::MockPrinter::Create(script.value());
}

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_string(descriptors_path, "", "Path to descriptors JSON file");
  DEFINE_string(record_doc_path, "", "Path to file to record document to");
  DEFINE_string(attributes_path, "", "Path to IPP attributes JSON file");
  DEFINE_string(scanner_capabilities_path, "",
                "Path to eSCL ScannerCapabilities JSON file");
  DEFINE_string(mock_printer_script, "",
                "Path to `mocking::TestCase` textproto for mock printer");
  DEFINE_string(http_header_output_dir, "",
                "Path to the directory for writing out http header info");
  DEFINE_string(output_log_dir, "",
                "Path to the directory for writing out log info");

  brillo::FlagHelper::Init(argc, argv, "Virtual USB Printer");
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  if (FLAGS_descriptors_path.empty()) {
    LOG(ERROR) << kUsage;
    return 1;
  }

  std::optional<UsbDescriptors> usb_descriptors =
      UsbDescriptors::Create(FLAGS_descriptors_path);
  if (!usb_descriptors.has_value()) {
    LOG(ERROR) << "Unable to create USB descriptors from file "
               << FLAGS_descriptors_path;
    return 1;
  }

  std::unique_ptr<mock_printer::MockPrinter> mock_printer;
  if (!FLAGS_mock_printer_script.empty()) {
    mock_printer = InitializeMockPrinter(FLAGS_mock_printer_script);
    if (!mock_printer) {
      LOG(ERROR) << "Failed to initialize mock printer";
      return 1;
    }
  }

  base::FilePath document_output_path(FLAGS_record_doc_path);

  IppManager ipp_manager;
  // Load ipp attributes if |FLAGS_attributes_path| is set.
  std::optional<base::Value> result;
  if (!FLAGS_attributes_path.empty()) {
    std::optional<std::string> attributes_contents =
        GetJSONContents(FLAGS_attributes_path);
    if (!attributes_contents.has_value()) {
      LOG(ERROR) << "Failed to load file contents for "
                 << FLAGS_attributes_path;
      return 1;
    }
    result = base::JSONReader::Read(*attributes_contents);
    if (!result) {
      LOG(ERROR) << "Failed to parse " << FLAGS_attributes_path;
      return 1;
    }
    if (!result->is_dict()) {
      LOG(ERROR) << "Failed to retrieve dictionary value from attributes";
      return 1;
    }

    std::vector<IppAttribute> operation_attributes =
        GetAttributes(result->GetDict(), kOperationAttributes);
    std::vector<IppAttribute> printer_attributes =
        GetAttributes(result->GetDict(), kPrinterAttributes);
    std::vector<IppAttribute> job_attributes =
        GetAttributes(result->GetDict(), kJobAttributes);
    std::vector<IppAttribute> unsupported_attributes =
        GetAttributes(result->GetDict(), kUnsupportedAttributes);

    ipp_manager =
        IppManager(operation_attributes, printer_attributes, job_attributes,
                   unsupported_attributes, document_output_path);
  }

  base::FilePath log_dir;
  if (!FLAGS_output_log_dir.empty()) {
    base::FilePath output_log_path(FLAGS_output_log_dir);
    if (!base::DirectoryExists(output_log_path)) {
      LOG(ERROR) << "Directory doesn't exist.";
      return 1;
    }
    log_dir = output_log_path;
  } else {
    log_dir = base::FilePath();
  }

  base::FilePath http_log_dir;
  if (!FLAGS_http_header_output_dir.empty()) {
    base::FilePath http_output_log_path(FLAGS_http_header_output_dir);
    if (!base::DirectoryExists(http_output_log_path)) {
      LOG(ERROR) << "Directory doesn't exist: " << http_output_log_path;
      return 1;
    }
    http_log_dir = http_output_log_path;
  }

  std::optional<EsclManager> escl_manager =
      InitializeEsclManager(FLAGS_scanner_capabilities_path, log_dir);
  if (!escl_manager.has_value())
    return 1;

  UsbPrinter printer(usb_descriptors.value(), document_output_path,
                     http_log_dir, std::move(ipp_manager),
                     std::move(escl_manager.value()), std::move(mock_printer));

  Server server(std::move(printer));
  server.Run();
}
