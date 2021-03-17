// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_matching_validation.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/logging.h"

namespace mock_printer {

namespace {

// Denotes what field is populated in a `mocking::IppAttribute`.
// Note that at most one field (besides the name) can be populated.
enum class IppAttributePopulatedField {
  kNone,
  kMultiple,  // this would be invalid.
  kCollections,
  kInts,
  kStrings,
  kIntRanges,
  kResolutions,
};

// Collections contain attributes but attributes may then contain
// collections; this circular hierarchy forces us to declare this
// helper validator up front.
ProtobufValidationResult ValidateIppCollection(
    const mocking::IppCollection& ipp_collection);

ProtobufValidationResult ValidateEmbeddedCollection(
    const mocking::IppAttribute& ipp_attribute) {
  for (const mocking::IppCollection& collection : ipp_attribute.collections()) {
    ProtobufValidationResult result = ValidateIppCollection(collection);
    if (!result.IsValid()) {
      return result;
    }
  }
  return ProtobufValidationResult{};
}

// Returns which field is populated in `ipp_attribute`.
// Note the `kMultiple` variant, which is non-specific but always an
// error case.
IppAttributePopulatedField GetPopulatedField(
    const mocking::IppAttribute& ipp_attribute) {
  IppAttributePopulatedField result = IppAttributePopulatedField::kNone;

  if (ipp_attribute.collections_size()) {
    result = result == IppAttributePopulatedField::kNone
                 ? IppAttributePopulatedField::kCollections
                 : IppAttributePopulatedField::kMultiple;
  }
  if (ipp_attribute.int_values_size()) {
    result = result == IppAttributePopulatedField::kNone
                 ? IppAttributePopulatedField::kInts
                 : IppAttributePopulatedField::kMultiple;
  }
  if (ipp_attribute.string_values_size()) {
    result = result == IppAttributePopulatedField::kNone
                 ? IppAttributePopulatedField::kStrings
                 : IppAttributePopulatedField::kMultiple;
  }
  if (ipp_attribute.int_ranges_size()) {
    result = result == IppAttributePopulatedField::kNone
                 ? IppAttributePopulatedField::kIntRanges
                 : IppAttributePopulatedField::kMultiple;
  }
  if (ipp_attribute.resolutions_size()) {
    result = result == IppAttributePopulatedField::kNone
                 ? IppAttributePopulatedField::kResolutions
                 : IppAttributePopulatedField::kMultiple;
  }

  return result;
}

ProtobufValidationResult ValidateResolutions(
    const mocking::IppAttribute& ipp_attribute) {
  for (const mocking::Resolution& resolution : ipp_attribute.resolutions()) {
    if (resolution.units() == mocking::Resolution::UNSPECIFIED) {
      return ProtobufValidationResult{"Resolution with Units::UNSPECIFIED"};
    } else if (!mocking::Resolution::Units_IsValid(resolution.units())) {
      return ProtobufValidationResult{base::StringPrintf(
          "Resolution with bad Units: ``%d''", resolution.units())};
    }
  }
  return ProtobufValidationResult{};
}

ProtobufValidationResult ValidateIppAttribute(
    const mocking::IppAttribute& ipp_attribute) {
  if (ipp_attribute.name().empty()) {
    return ProtobufValidationResult{"IppAttribute with empty name"};
  }

  const auto field = GetPopulatedField(ipp_attribute);
  switch (field) {
    case (IppAttributePopulatedField::kNone):
      return ProtobufValidationResult{};
    case (IppAttributePopulatedField::kMultiple):
      return ProtobufValidationResult{base::StringPrintf(
          "IppAttribute (with name ``%s'') has multiple populated fields",
          ipp_attribute.name().c_str())};
    case (IppAttributePopulatedField::kCollections):
      return ValidateEmbeddedCollection(ipp_attribute);
    // Ints, strings, and ranges-of-ints need no special handling.
    case (IppAttributePopulatedField::kInts):
    case (IppAttributePopulatedField::kStrings):
    case (IppAttributePopulatedField::kIntRanges):
      return ProtobufValidationResult{};
    case (IppAttributePopulatedField::kResolutions):
      return ValidateResolutions(ipp_attribute);
  }
}

ProtobufValidationResult ValidateIppCollection(
    const mocking::IppCollection& ipp_collection) {
  if (!ipp_collection.attributes_size()) {
    return ProtobufValidationResult{"empty IppCollection"};
  }

  for (const mocking::IppAttribute& attribute : ipp_collection.attributes()) {
    ProtobufValidationResult result = ValidateIppAttribute(attribute);
    if (!result.IsValid()) {
      return result;
    }
  }

  return ProtobufValidationResult{};
}

ProtobufValidationResult ValidateIppGroup(const mocking::IppGroup& ipp_group) {
  if (ipp_group.tag() == mocking::IppGroup::UNSPECIFIED) {
    return ProtobufValidationResult{"IppGroup with IppGroupTag::UNSPECIFIED"};
  } else if (!mocking::IppGroup::IppGroupTag_IsValid(ipp_group.tag())) {
    return ProtobufValidationResult{
        base::StringPrintf("invalid IppGroupTag: ``%d''", ipp_group.tag())};
  }

  for (const mocking::IppCollection& collection : ipp_group.collections()) {
    ProtobufValidationResult result = ValidateIppCollection(collection);
    if (!result.IsValid()) {
      return result;
    }
  }

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
