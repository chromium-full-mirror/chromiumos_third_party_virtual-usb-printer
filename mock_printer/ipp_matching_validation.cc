// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_matching_validation.h"

#include <cstdint>
#include <string>

#include <base/strings/stringprintf.h>
#include <base/strings/string_piece.h>
#include <base/logging.h>
#include <base/check.h>

namespace mock_printer {

namespace {

// Collections contain attributes but attributes may then contain
// collections; this circular hierarchy forces us to declare this
// helper validator up front.
ProtobufValidationResult ValidateIppCollection(
    const mocking::IppCollection& ipp_collection);

ProtobufValidationResult ValidateEmbeddedCollection(
    const mocking::IppAttribute& ipp_attribute) {
  for (const mocking::IppCollection& collection :
       ipp_attribute.collections().collections()) {
    ProtobufValidationResult result = ValidateIppCollection(collection);
    if (!result.IsValid()) {
      return result;
    }
  }
  return ProtobufValidationResult{};
}

ProtobufValidationResult ValidateResolutions(
    const mocking::IppAttribute& ipp_attribute) {
  for (const mocking::Resolution& resolution :
       ipp_attribute.resolutions().resolutions()) {
    if (resolution.units() == mocking::Resolution::UNSPECIFIED) {
      return ProtobufValidationResult{"Resolution with Units::UNSPECIFIED"};
    } else if (!mocking::Resolution::Units_IsValid(resolution.units())) {
      return ProtobufValidationResult{base::StringPrintf(
          "Resolution with bad Units: ``%d''", resolution.units())};
    }
  }
  return ProtobufValidationResult{};
}

// This helper function is called given a declared type for a
// `mocking::IppAttribute`. It's called with
// *  the type of the actual `populated_value`,
// *  the `expected_populated_value` (which should equal
//    `populated_value`),
// *  the true `values_size` of the populated value.
//
// This function returns whether the arguments indicate a well-formed
// `mocking::IppAttribute`.
ProtobufValidationResult EnsureAttributeTypeAndSize(
    const mocking::IppAttribute::ValueCase populated_value,
    const mocking::IppAttribute::ValueCase expected_populated_value,
    int32_t values_size) {
  // It is perfectly permissible to declare a type for matching but to
  // supply no values to match on.
  if (populated_value == mocking::IppAttribute::ValueCase::VALUE_NOT_SET) {
    return ProtobufValidationResult{};
  } else if (populated_value != expected_populated_value) {
    return ProtobufValidationResult{base::StringPrintf(
        // There appears to be no `ValueCase_Name()` function generated.
        "mismatched IppAttribute type: expected ``%d'' but populated ``%d''",
        expected_populated_value, populated_value)};
  }

  // Catches the case where
  // *  a value type is declared,
  // *  the value type matches,
  // *  but the container is empty.
  if (!values_size) {
    return ProtobufValidationResult{"empty value holder"};
  }
  return ProtobufValidationResult{};
}

// Verifies that a `mocking::IppAttribute` with a string-ish declared
// type actually contains either strings (with languages as appropriate)
// or nothing at all.
ProtobufValidationResult EnsureLanguageSetting(
    const mocking::IppAttribute::IppAttributeType declared_type,
    const mocking::RepeatedStringValues& string_values) {
  // The previous call to `EnsureAttributeTypeAndSize()` must
  // guarantee that `string_values.string_values(0)` is safe to call.
  CHECK(string_values.string_values_size())
      << "BUG: empty `RepeatedStringValues`";
  const bool language_required =
      (declared_type == mocking::IppAttribute::TEXT_WITH_LANGUAGE) ||
      (declared_type == mocking::IppAttribute::NAME_WITH_LANGUAGE);
  const std::string language = string_values.string_values(0).language();

  if (language_required && language.empty()) {
    return ProtobufValidationResult{"language required but not given"};
  }
  for (const mocking::OctetString& value : string_values.string_values()) {
    if (value.language() != language) {
      return ProtobufValidationResult{base::StringPrintf(
          "language mismatch: expected ``%s'' but got ``%s''", language.c_str(),
          value.language().c_str())};
    }
  }
  return ProtobufValidationResult{};
}

// Verifies that a `mocking::IppAttribute` with a string-ish declared
// type contains no empty `OctetString` messages.
//
// Language validation is not done here; see `EnsureLanguageSetting()`.
ProtobufValidationResult EnsureOctetStringsArePopulated(
    const mocking::RepeatedStringValues& string_values) {
  for (const mocking::OctetString& octet_string :
       string_values.string_values()) {
    if (octet_string.value().empty()) {
      return ProtobufValidationResult{"`OctetString` message with no `value`"};
    }
  }
  return ProtobufValidationResult{};
}

// Ensures that the declared type of `ipp_attribute` matches what's
// actually populated in the `oneof value` field.
//
// This boilerplate is painful to read because while there are many
// possible declared value types for a `mocking::IppAttribute`, they
// currently map onto far fewer value "holders" in the `oneof value`.
// Generally:
// *  While the `type` field is optional, it must be given if any values
//    are populated.
// *  The `type` field must map properly onto the actually supplied
//    values - e.g. it is invalid to declare an integral type and supply
//    strings.
// *  The `oneof value` field can be altogether empty, but it cannot be
//    "partially empty;" e.g. it is illegal to directly supply an empty
//    `RepeatedIntValues`.
//
// TODO(b/178649877): add validation for
// mocking::IppAttribute::DATETIME.
ProtobufValidationResult EnsureAttributeValueMatchesType(
    const mocking::IppAttribute& ipp_attribute) {
  const auto declared_type = ipp_attribute.type();
  const auto populated_value = ipp_attribute.value_case();

  // For a given `ipp_attribute`, we mainly check that the declared type
  // squares with the populated value:
  // *  no values may be populated without a declared type, and
  // *  a declared type must be reasonable for the populated value.
  switch (declared_type) {
    case (mocking::IppAttribute::UNSPECIFIED):
      return populated_value == mocking::IppAttribute::ValueCase::VALUE_NOT_SET
                 ? ProtobufValidationResult{}
                 : ProtobufValidationResult{
                       "IppAttribute with unset type but populated value "
                       "field"};
    case (mocking::IppAttribute::INTEGER):
    case (mocking::IppAttribute::BOOLEAN):
    case (mocking::IppAttribute::ENUM):
      return EnsureAttributeTypeAndSize(
          populated_value, mocking::IppAttribute::ValueCase::kIntValues,
          ipp_attribute.int_values().int_values_size());
    case (mocking::IppAttribute::OCTET_STRING):
    case (mocking::IppAttribute::TEXT_WITH_LANGUAGE):
    case (mocking::IppAttribute::NAME_WITH_LANGUAGE):
    case (mocking::IppAttribute::TEXT_WITHOUT_LANGUAGE):
    case (mocking::IppAttribute::NAME_WITHOUT_LANGUAGE):
    case (mocking::IppAttribute::KEYWORD):
    case (mocking::IppAttribute::URI):
    case (mocking::IppAttribute::URI_SCHEME):
    case (mocking::IppAttribute::CHARSET):
    case (mocking::IppAttribute::NATURAL_LANGUAGE):
    case (mocking::IppAttribute::MIME_MEDIA_TYPE):
    case (mocking::IppAttribute::MEMBER_ATTRIBUTE_NAME): {
      const auto type_check = EnsureAttributeTypeAndSize(
          populated_value, mocking::IppAttribute::ValueCase::kStringValues,
          ipp_attribute.string_values().string_values_size());
      // `EnsureAttributeTypeAndSize()` only checks that the oneof
      // `value` is either unset or contains a non-empty
      // `RepeatedStringValues` message.
      //
      // We need to ensure that the `OctetString` messages are not empty,
      // since that's also an invalid construct.
      if (!type_check.IsValid() ||
          populated_value == mocking::IppAttribute::ValueCase::VALUE_NOT_SET) {
        return type_check;
      }
      const auto emptiness_check =
          EnsureOctetStringsArePopulated(ipp_attribute.string_values());
      if (!emptiness_check.IsValid()) {
        return emptiness_check;
      }
      return EnsureLanguageSetting(declared_type,
                                   ipp_attribute.string_values());
    }
    case (mocking::IppAttribute::RESOLUTION):
      return EnsureAttributeTypeAndSize(
          populated_value, mocking::IppAttribute::ValueCase::kResolutions,
          ipp_attribute.resolutions().resolutions_size());
    case (mocking::IppAttribute::RANGE_OF_INTEGER):
      return EnsureAttributeTypeAndSize(
          populated_value, mocking::IppAttribute::ValueCase::kIntRanges,
          ipp_attribute.int_ranges().int_ranges_size());
    case (mocking::IppAttribute::BEGIN_COLLECTION):
      return EnsureAttributeTypeAndSize(
          populated_value, mocking::IppAttribute::ValueCase::kCollections,
          ipp_attribute.collections().collections_size());
    default:
      return ProtobufValidationResult{base::StringPrintf(
          "IppAttribute with unexpected type %x", ipp_attribute.type())};
  }
}

ProtobufValidationResult ValidateIppAttribute(
    const mocking::IppAttribute& ipp_attribute) {
  if (ipp_attribute.name().empty()) {
    return ProtobufValidationResult{"IppAttribute with empty name"};
  }

  // Ensures that the declared type (if given) matches the populated
  // value.
  const auto result = EnsureAttributeValueMatchesType(ipp_attribute);
  if (!result.IsValid()) {
    return result;
  }

  switch (ipp_attribute.type()) {
    case (mocking::IppAttribute::BEGIN_COLLECTION):
      return ValidateEmbeddedCollection(ipp_attribute);
    case (mocking::IppAttribute::RESOLUTION):
      return ValidateResolutions(ipp_attribute);
    default:
      return ProtobufValidationResult{};
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
