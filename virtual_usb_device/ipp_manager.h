// Copyright 2020 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_IPP_MANAGER_H_
#define VIRTUAL_USB_DEVICE_IPP_MANAGER_H_

#include <vector>
#include <memory>
#include <string>

#include <base/files/file_path.h>

#include "ipp_util.h"
#include "virtual-usb-printer/common/smart_buffer.h"

// This class is responsible for generating responses to IPP requests sent over
// USB.
class IppManager {
 public:
  // Create IppManager after reading ipp attribute contents the
  // `ipp_attributs_path` config file.
  static std::unique_ptr<IppManager> Create(
      const std::string& ipp_attributes_path,
      const base::FilePath& document_output_path);
  IppManager() = default;
  IppManager(std::vector<IppAttribute>& operation_attributes,
             std::vector<IppAttribute>& printer_attributes,
             std::vector<IppAttribute>& job_attributes,
             std::vector<IppAttribute>& unsupported_attributes,
             std::optional<base::Value> attributes_data,
             const base::FilePath& document_output_path);

  // Returns a standard response based on the operation specified in
  // |ipp_header|.
  SmartBuffer HandleIppRequest(const IppHeader& ipp_header,
                               const SmartBuffer& body) const;

  // Result returned in the |operation_id| field of an IppHeader when the
  // operation was successful.
  static const uint16_t kSuccessStatus;

 private:
  // Handles `IPP_VALIDATE_JOB` ipp operation.
  SmartBuffer HandleValidateJob(const IppHeader& request_header) const;
  // Handles `IPP_CREATE_JOB` ipp operation.
  SmartBuffer HandleCreateJob(const IppHeader& request_header) const;
  // Handles `IPP_SEND_DOCUMENT` ipp operation.
  SmartBuffer HandleSendDocument(const IppHeader& request_header,
                                 const SmartBuffer& body) const;
  // Handles `IPP_GET_JOB_ATTRIBUTES` ipp operation.
  SmartBuffer HandleGetJobAttributes(const IppHeader& request_header) const;
  // Handles `IPP_GET_PRINTER_ATTRIBUTES` ipp operation.
  SmartBuffer HandleGetPrinterAttributes(const IppHeader& ipp_header) const;

  // Constant attributes that will be returned in response to requests from the
  // client.
  std::vector<IppAttribute> operation_attributes_;
  std::vector<IppAttribute> printer_attributes_;
  std::vector<IppAttribute> job_attributes_;

  // TODO(valleau): Look into making these attributes dynamic as we should only
  // report unsupported attributes if they were requested by the client.
  std::vector<IppAttribute> unsupported_attributes_;

  std::optional<base::Value> attributes_data_;

  base::FilePath document_output_path_;
};

#endif  // VIRTUAL_USB_DEVICE_IPP_MANAGER_H_
