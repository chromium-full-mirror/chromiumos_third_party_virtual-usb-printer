// Copyright 2020 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_manager.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/values.h>
#include <chromeos/libipp/ipp_enums.h>

#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/common/value_util.h"
#include "virtual-usb-printer/virtual_usb_device/ipp_util.h"
#include "virtual-usb-printer/virtual_usb_device/usbip_constants.h"

const uint16_t IppManager::kSuccessStatus = 0;

std::unique_ptr<IppManager> IppManager::Create(
    const std::string& ipp_attributes_path,
    const base::FilePath& document_output_path) {
  if (ipp_attributes_path.empty())
    return std::make_unique<IppManager>();

  std::optional<std::string> attributes_contents =
      GetJSONContents(ipp_attributes_path);
  if (!attributes_contents.has_value()) {
    LOG(ERROR) << "Failed to load file contents for " << ipp_attributes_path;
    return nullptr;
  }
  // The values inside the `attributes_data_` are referenced as pointer all
  // the way, so this must exists till this object is alive.
  std::optional<base::Value> attributes_data = base::JSONReader::Read(
      *attributes_contents, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!attributes_data) {
    LOG(ERROR) << "Failed to parse " << ipp_attributes_path;
    return nullptr;
  }
  if (!attributes_data->is_dict()) {
    LOG(ERROR) << "Failed to retrieve dictionary value from attributes";
    return nullptr;
  }

  std::vector<IppAttribute> operation_attributes =
      GetAttributes(attributes_data->GetDict(), kOperationAttributes);
  std::vector<IppAttribute> printer_attributes =
      GetAttributes(attributes_data->GetDict(), kPrinterAttributes);
  std::vector<IppAttribute> job_attributes =
      GetAttributes(attributes_data->GetDict(), kJobAttributes);
  std::vector<IppAttribute> unsupported_attributes =
      GetAttributes(attributes_data->GetDict(), kUnsupportedAttributes);

  return std::make_unique<IppManager>(
      operation_attributes, printer_attributes, job_attributes,
      unsupported_attributes, std::move(attributes_data), document_output_path);
}

IppManager::IppManager(std::vector<IppAttribute>& operation_attributes,
                       std::vector<IppAttribute>& printer_attributes,
                       std::vector<IppAttribute>& job_attributes,
                       std::vector<IppAttribute>& unsupported_attributes,
                       std::optional<base::Value> attributes_data,
                       const base::FilePath& document_output_path)
    : operation_attributes_(operation_attributes),
      printer_attributes_(printer_attributes),
      job_attributes_(job_attributes),
      unsupported_attributes_(unsupported_attributes),
      attributes_data_(std::move(attributes_data)),
      document_output_path_(document_output_path) {}

SmartBuffer IppManager::HandleIppRequest(const IppHeader& ipp_header,
                                         const SmartBuffer& body) const {
  ipp::E_operations_supported op =
      static_cast<ipp::E_operations_supported>(ipp_header.operation_id);
  LOG(INFO) << base::StringPrintf("IPP/%d.%d %s", ipp_header.major,
                                  ipp_header.minor, ipp::ToString(op).c_str());

  switch (ipp_header.operation_id) {
    case IPP_VALIDATE_JOB:
      return HandleValidateJob(ipp_header);
    case IPP_CREATE_JOB:
      return HandleCreateJob(ipp_header);
    case IPP_SEND_DOCUMENT:
      return HandleSendDocument(ipp_header, body);
    case IPP_GET_JOB_ATTRIBUTES:
      return HandleGetJobAttributes(ipp_header);
    case IPP_GET_PRINTER_ATTRIBUTES:
      return HandleGetPrinterAttributes(ipp_header);
    default:
      LOG(ERROR) << "Unknown operation id in ipp request "
                 << ipp_header.operation_id;
      return SmartBuffer();
  }
}

SmartBuffer IppManager::HandleValidateJob(
    const IppHeader& request_header) const {
  VLOG(1) << "HandleValidateJob " << request_header.request_id;

  IppHeader response_header = request_header;
  response_header.operation_id = kSuccessStatus;
  // We add 1 to the size for the end of attributes tag.
  size_t response_size =
      sizeof(response_header) + GetAttributesSize(operation_attributes_) + 1;
  SmartBuffer buf(response_size);
  response_header.Serialize(&buf);
  AddPrinterAttributes(operation_attributes_, kOperationAttributes, &buf);
  AddEndOfAttributes(&buf);
  return buf;
}

SmartBuffer IppManager::HandleCreateJob(const IppHeader& request_header) const {
  LOG(INFO) << "HandleCreateJob " << request_header.request_id;

  IppHeader response_header = request_header;
  response_header.operation_id = kSuccessStatus;
  // We add 1 to the size for the end of attributes tag.
  size_t response_size = sizeof(response_header) +
                         GetAttributesSize(operation_attributes_) +
                         GetAttributesSize(job_attributes_) + 1;
  SmartBuffer buf(response_size);
  response_header.Serialize(&buf);
  AddPrinterAttributes(operation_attributes_, kOperationAttributes, &buf);
  AddPrinterAttributes(job_attributes_, kJobAttributes, &buf);
  AddEndOfAttributes(&buf);
  return buf;
}

SmartBuffer IppManager::HandleSendDocument(const IppHeader& request_header,
                                           const SmartBuffer& body) const {
  LOG(INFO) << "HandleSendDocument " << request_header.request_id;

  if (!document_output_path_.empty()) {
    LOG(INFO) << "Recording document...";
    if (!base::WriteFile(document_output_path_, body.contents())) {
      PLOG(ERROR) << "Failed to write document to file: "
                  << document_output_path_;
    }
  }

  IppHeader response_header = request_header;
  response_header.operation_id = kSuccessStatus;
  // We add 1 to the size for the end of attributes tag.
  size_t response_size = sizeof(response_header) +
                         GetAttributesSize(operation_attributes_) +
                         GetAttributesSize(job_attributes_) + 1;
  SmartBuffer buf(response_size);
  response_header.Serialize(&buf);
  AddPrinterAttributes(operation_attributes_, kOperationAttributes, &buf);
  AddPrinterAttributes(job_attributes_, kJobAttributes, &buf);
  AddEndOfAttributes(&buf);
  return buf;
}

SmartBuffer IppManager::HandleGetJobAttributes(
    const IppHeader& request_header) const {
  LOG(INFO) << "HandleGetJobAttributes " << request_header.request_id;
  IppHeader response_header = request_header;
  response_header.operation_id = kSuccessStatus;
  // We add 1 to the size for the end of attributes tag.
  size_t response_size = sizeof(response_header) +
                         GetAttributesSize(operation_attributes_) +
                         GetAttributesSize(job_attributes_) + 1;
  SmartBuffer buf(response_size);
  response_header.Serialize(&buf);
  AddPrinterAttributes(operation_attributes_, kOperationAttributes, &buf);
  AddPrinterAttributes(job_attributes_, kJobAttributes, &buf);
  AddEndOfAttributes(&buf);
  return buf;
}

SmartBuffer IppManager::HandleGetPrinterAttributes(
    const IppHeader& request_header) const {
  LOG(INFO) << "HandleGetPrinterAttributes " << request_header.request_id;

  IppHeader response_header = request_header;
  response_header.operation_id = kSuccessStatus;
  // We add 1 to the size for the end of attributes tag.
  size_t response_size = sizeof(response_header) +
                         GetAttributesSize(operation_attributes_) +
                         GetAttributesSize(printer_attributes_) + 1;
  SmartBuffer buf(response_size);
  response_header.Serialize(&buf);
  AddPrinterAttributes(operation_attributes_, kOperationAttributes, &buf);
  AddPrinterAttributes(printer_attributes_, kPrinterAttributes, &buf);
  AddEndOfAttributes(&buf);
  return buf;
}
