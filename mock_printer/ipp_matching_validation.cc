// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_matching_validation.h"

#include <string>

#include "base/strings/stringprintf.h"

namespace mock_printer {

namespace {

ProtobufValidationResult ValidateIppGroup(const mocking::IppGroup& ipp_group) {
  if (ipp_group.tag() == mocking::IppGroup::UNSPECIFIED) {
    return ProtobufValidationResult{"IppGroup with IppGroupTag::UNSPECIFIED"};
  } else if (!mocking::IppGroup::IppGroupTag_IsValid(ipp_group.tag())) {
    return ProtobufValidationResult{
        base::StringPrintf("invalid IppGroupTag: ``%d''", ipp_group.tag())};
  }

  // TODO(b/178649877): descend into IppGroup::collections and validate
  // everything below.

  return ProtobufValidationResult{};
}

}  // namespace

ProtobufValidationResult ValidateIppMessage(
    const mocking::IppMessage& ipp_message) {
  // Operation IDs may be [1, 2**31 - 1]; see
  // https://tools.ietf.org/html/rfc8011#section-4.1.2
  if (ipp_message.operation_id() <= 0) {
    return ProtobufValidationResult{
        base::StringPrintf("IppMessage has invalid operation_id: ``%d''",
                           ipp_message.operation_id())};
  }

  if (ipp_message.groups_size() < 1) {
    return ProtobufValidationResult{
        "IppMessage requires at least one IppGroup"};
  }

  for (const mocking::IppGroup& group : ipp_message.groups()) {
    ProtobufValidationResult result = ValidateIppGroup(group);
    if (!result.IsValid()) {
      return result;
    }
  }

  return ProtobufValidationResult{};
}

}  // namespace mock_printer
