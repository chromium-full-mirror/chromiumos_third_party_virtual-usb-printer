// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_IPP_MATCHING_VALIDATION_H_
#define MOCK_PRINTER_IPP_MATCHING_VALIDATION_H_

// This header declares functions used to validate the IPP Protobufs
// given to virtual-usb-printer for matching.

#include <string>

#include "mock_printer/proto/ipp.pb.h"

namespace mock_printer {

// Returned by validation functions.
struct ProtobufValidationResult {
  bool IsValid() const { return error_message.empty(); }

  // Empty when the Protobuf is valid. Contains the validation error
  // when the Protobuf is invalid.
  std::string error_message;
};

// Public function for validating IPP message Protobufs.
// *  Descends the IPP hierarchy and validates all contents of
//    `ipp_message`.
// *  Enforces optional / required fields for _matching_. No facility
//    is provided to validate fields used for building responses.
ProtobufValidationResult ValidateIppMessage(
    const mocking::IppMessage& ipp_message);

}  // namespace mock_printer

#endif  // MOCK_PRINTER_IPP_MATCHING_VALIDATION_H_
