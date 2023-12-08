// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_matching_validation.h"

#include <cstdint>
#include <string>

#include <base/strings/stringprintf.h>
#include <base/logging.h>
#include <base/check.h>
#include <base/check_op.h>
#include <base/containers/fixed_flat_set.h>

namespace mock_printer {

namespace {

// Defines the set of `mocking::IppAttribute::IppAttributeType`
// that are considered strings.
constexpr auto kMockAttributeStringTypes =
    base::MakeFixedFlatSet<mocking::IppAttribute::IppAttributeType>(
        {mocking::IppAttribute::OCTET_STRING,
         mocking::IppAttribute::TEXT_WITH_LANGUAGE,
         mocking::IppAttribute::NAME_WITH_LANGUAGE,
         mocking::IppAttribute::TEXT_WITHOUT_LANGUAGE,
         mocking::IppAttribute::NAME_WITHOUT_LANGUAGE,
         mocking::IppAttribute::KEYWORD, mocking::IppAttribute::URI,
         mocking::IppAttribute::URI_SCHEME, mocking::IppAttribute::CHARSET,
         mocking::IppAttribute::NATURAL_LANGUAGE,
         mocking::IppAttribute::MIME_MEDIA_TYPE,
         mocking::IppAttribute::MEMBER_ATTRIBUTE_NAME});

bool IsStringType(const mocking::IppAttribute::IppAttributeType type) {
  return kMockAttributeStringTypes.contains(type);
}

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

ProtobufValidationResult ValidateDateTimes(
    const mocking::IppAttribute& ipp_attribute) {
  for (const mocking::DateTime& date_time :
       ipp_attribute.date_times().date_times()) {
    // No particular enforcement is provided for bogus values; we're
    // most interested in weeding out spurious empty messages.
    if (!date_time.ByteSizeLong()) {
      return ProtobufValidationResult{"empty `mocking::DateTime`"};
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
  CHECK_NE(populated_value, mocking::IppAttribute::ValueCase::VALUE_NOT_SET)
      << "BUG: unset value not expected here";
  if (populated_value != expected_populated_value) {
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
ProtobufValidationResult EnsureAttributeValueMatchesType(
    const mocking::IppAttribute& ipp_attribute) {
  const auto declared_type = ipp_attribute.type();
  const auto populated_value = ipp_attribute.value_case();

  // For a given `ipp_attribute`, we mainly check that the declared type
  // squares with the populated value:
  // *  no values may be populated without a declared type, and
  // *  a declared type must be reasonable for the populated value.
  //
  // We handle string types up front because there are a lot of them,
  // which would otherwise clutter up the switch statement.
  if (IsStringType(declared_type)) {
    // `EnsureAttributeTypeAndSize()` only checks that the oneof
    // `value` contains a non-empty `RepeatedStringValues` message.
    //
    // Even if the type and value match and the value is not empty, an
    // additional check for non-empty `mocking::OctetString` messages
    // (also deemed an invalid construct) is needed but not performed
    // here.
    return EnsureAttributeTypeAndSize(
        populated_value, mocking::IppAttribute::ValueCase::kStringValues,
        ipp_attribute.string_values().string_values_size());
  }

  switch (declared_type) {
    case (mocking::IppAttribute::INTEGER):
    case (mocking::IppAttribute::BOOLEAN):
    case (mocking::IppAttribute::ENUM):
      return EnsureAttributeTypeAndSize(
          populated_value, mocking::IppAttribute::ValueCase::kIntValues,
          ipp_attribute.int_values().int_values_size());
    case (mocking::IppAttribute::DATETIME):
      return EnsureAttributeTypeAndSize(
          populated_value, mocking::IppAttribute::ValueCase::kDateTimes,
          ipp_attribute.date_times().date_times_size());
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

  // If no value is provided, there's nothing to validate; the mock
  // attribute becomes a wildcard matcher whose type (if given) and
  // name are the only things that matter.
  if (ipp_attribute.value_case() ==
      mocking::IppAttribute::ValueCase::VALUE_NOT_SET) {
    return ProtobufValidationResult{};
  }

  // Ensures that the declared type (if given) matches the populated
  // value.
  const auto result = EnsureAttributeValueMatchesType(ipp_attribute);
  if (!result.IsValid()) {
    return result;
  }

  if (IsStringType(ipp_attribute.type())) {
    const auto emptiness_check =
        EnsureOctetStringsArePopulated(ipp_attribute.string_values());
    if (!emptiness_check.IsValid()) {
      return emptiness_check;
    }
    return EnsureLanguageSetting(ipp_attribute.type(),
                                 ipp_attribute.string_values());
  }

  switch (ipp_attribute.type()) {
    case (mocking::IppAttribute::BEGIN_COLLECTION):
      return ValidateEmbeddedCollection(ipp_attribute);
    case (mocking::IppAttribute::RESOLUTION):
      return ValidateResolutions(ipp_attribute);
    case (mocking::IppAttribute::DATETIME):
      return ValidateDateTimes(ipp_attribute);
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
  //
  // This really only catches empty values; there might be invalid ones
  // that we simply won't understand.
  if (!ipp_message.operation_id()) {
    return ProtobufValidationResult{"IppMessage has no operation_id"};
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
