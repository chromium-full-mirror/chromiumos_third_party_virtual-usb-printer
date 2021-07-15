// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "proto_to_libipp.h"

#include <string>
#include <cstddef>

#include <base/check.h>
#include <base/check_op.h>
#include <base/notreached.h>
#include <base/containers/fixed_flat_map.h>
#include <chromeos/libipp/ipp_enums.h>

namespace mock_printer {

namespace {

void CopyAttributes(const mocking::IppCollection& mock_collection,
                    ipp::Collection* collection);

int AttributeSize(const mocking::IppAttribute& mock_attribute) {
  switch (mock_attribute.value_case()) {
    case mocking::IppAttribute::ValueCase::kCollections:
      return mock_attribute.collections().collections_size();
    case mocking::IppAttribute::ValueCase::kIntValues:
      return mock_attribute.int_values().int_values_size();
    case mocking::IppAttribute::ValueCase::kStringValues:
      return mock_attribute.string_values().string_values_size();
    case mocking::IppAttribute::ValueCase::kIntRanges:
      return mock_attribute.int_ranges().int_ranges_size();
    case mocking::IppAttribute::ValueCase::kResolutions:
      return mock_attribute.resolutions().resolutions_size();
    case mocking::IppAttribute::ValueCase::kDateTimes:
      return mock_attribute.date_times().date_times_size();
    default:
      NOTREACHED() << "BUG: unhandled `mocking::IppAttribute` value case";
  }
  return -1;  // not reached
}

// Adds a nested collection full of attributes from `mock_attribute`
// into `attribute`.
//
// Do not confuse this function for adding IPP collections into
// IPP groups: because of the way libipp is structured, such a function
// is unnecessary in this context.
void CopyCollections(const mocking::IppAttribute& mock_attribute,
                     ipp::Attribute* attribute) {
  std::size_t i = 0;
  for (const mocking::IppCollection& mock_collection :
       mock_attribute.collections().collections()) {
    ipp::Collection* collection = attribute->GetCollection(i);
    CopyAttributes(mock_collection, collection);
    ++i;
  }
}

// Populates `attribute` with the integer values from `mock_attribute`.
void CopyInts(const mocking::IppAttribute& mock_attribute,
              ipp::Attribute* attribute) {
  std::size_t i = 0;
  for (const int32_t mock_int : mock_attribute.int_values().int_values()) {
    attribute->SetValue(mock_int, i++);
  }
}

// Populates `attribute` with the string values from `mock_attribute`.
void CopyStrings(const mocking::IppAttribute& mock_attribute,
                 ipp::Attribute* attribute) {
  std::size_t i = 0;
  switch (mock_attribute.type()) {
    case mocking::IppAttribute::TEXT_WITH_LANGUAGE:
    case mocking::IppAttribute::NAME_WITH_LANGUAGE:
      for (const mocking::OctetString& mock_string :
           mock_attribute.string_values().string_values()) {
        CHECK(
            attribute->SetValue(ipp::StringWithLanguage(mock_string.language(),
                                                        mock_string.value()),
                                i++));
      }
      break;
    default:
      for (const mocking::OctetString& mock_string :
           mock_attribute.string_values().string_values()) {
        CHECK(attribute->SetValue(mock_string.value(), i++));
      }
      break;
  }
}

// Populates `attribute` with the integer ranges from `mock_attribute`.
void CopyIntRanges(const mocking::IppAttribute& mock_attribute,
                   ipp::Attribute* attribute) {
  std::size_t i = 0;
  for (const mocking::IntegerRange& mock_range :
       mock_attribute.int_ranges().int_ranges()) {
    attribute->SetValue(
        ipp::RangeOfInteger(mock_range.minimum(), mock_range.maximum()), i++);
  }
}

// Populates `attribute` with the resolutions from `mock_attribute`.
void CopyResolutions(const mocking::IppAttribute& mock_attribute,
                     ipp::Attribute* attribute) {
  std::size_t i = 0;
  for (const mocking::Resolution& mock_resolution :
       mock_attribute.resolutions().resolutions()) {
    attribute->SetValue(
        ipp::Resolution(
            mock_resolution.xres(), mock_resolution.yres(),
            mock_resolution.units() == mocking::Resolution::DOTS_PER_CENTIMETER
                ? ipp::Resolution::Units::kDotsPerCentimeter
                : ipp::Resolution::Units::kDotsPerInch),
        i++);
  }
}

// Populates `attribute` with the datetime objects from `mock_attribute`.
void CopyDateTimes(const mocking::IppAttribute& mock_attribute,
                   ipp::Attribute* attribute) {
  std::size_t i = 0;
  for (const mocking::DateTime& mock_datetime :
       mock_attribute.date_times().date_times()) {
    attribute->SetValue(
        ipp::DateTime{
            .year = static_cast<uint16_t>(mock_datetime.year()),
            .month = static_cast<uint8_t>(mock_datetime.month()),
            .day = static_cast<uint8_t>(mock_datetime.day()),
            .hour = static_cast<uint8_t>(mock_datetime.hour()),
            .minutes = static_cast<uint8_t>(mock_datetime.minutes()),
            .seconds = static_cast<uint8_t>(mock_datetime.seconds()),
            .deci_seconds = static_cast<uint8_t>(mock_datetime.deciseconds()),
            .UTC_direction =
                mock_datetime.utc_direction() == mocking::DateTime::PLUS
                    ? static_cast<uint8_t>('+')
                    : static_cast<uint8_t>('-'),
            .UTC_hours = static_cast<uint8_t>(mock_datetime.utc_hours()),
            .UTC_minutes = static_cast<uint8_t>(mock_datetime.utc_minutes())},
        i++);
  }
}

// Iterates over `mock_collection` and adds all attributes to
// `collection`.
void CopyAttributes(const mocking::IppCollection& mock_collection,
                    ipp::Collection* collection) {
  for (const auto& mock_attribute : mock_collection.attributes()) {
    ipp::Attribute* attribute = collection->GetAttribute(mock_attribute.name());
    if (!attribute) {
      attribute = collection->AddUnknownAttribute(
          mock_attribute.name(), /*is_a_set=*/AttributeSize(mock_attribute) > 1,
          static_cast<ipp::AttrType>(mock_attribute.type()));
    }
    CHECK(attribute) << "BUG: expected `ipp::Attribute*` here";
    attribute->Resize(AttributeSize(mock_attribute));

    switch (mock_attribute.value_case()) {
      case mocking::IppAttribute::ValueCase::kCollections:
        return CopyCollections(mock_attribute, attribute);
      case mocking::IppAttribute::ValueCase::kIntValues:
        return CopyInts(mock_attribute, attribute);
      case mocking::IppAttribute::ValueCase::kStringValues:
        return CopyStrings(mock_attribute, attribute);
      case mocking::IppAttribute::ValueCase::kIntRanges:
        return CopyIntRanges(mock_attribute, attribute);
      case mocking::IppAttribute::ValueCase::kResolutions:
        return CopyResolutions(mock_attribute, attribute);
      case mocking::IppAttribute::ValueCase::kDateTimes:
        return CopyDateTimes(mock_attribute, attribute);
      default:
        NOTREACHED() << "BUG: unhandled `mocking::IppAttribute` value case";
    }
  }
}

// Populates `response` with the contents of `mock_group`.
void CopyGroup(const mocking::IppGroup& mock_group, ipp::Response* response) {
  const bool is_a_set = mock_group.collections_size() > 1;
  const auto tag = mock_group.tag();

  CHECK_GE(tag, 0) << "group tag " << tag << " < 0";
  CHECK_LE(tag, 0x0f) << "group tag " << tag << " > 0x0f";

  ipp::Group* group = response->GetGroup(static_cast<ipp::GroupTag>(tag));
  if (!group) {
    group =
        response->AddUnknownGroup(static_cast<ipp::GroupTag>(tag), is_a_set);
  }
  CHECK(group) << "BUG: expected a group here";

  group->Resize(mock_group.collections_size());

  std::size_t i = 0;
  for (const auto& mock_collection : mock_group.collections()) {
    CopyAttributes(mock_collection, group->GetCollection(i++));
  }
}

}  // namespace

std::unique_ptr<ipp::Response> ToResponse(
    const mocking::IppMessage& mock_response) {
  auto response = ipp::Response::NewResponse(
      static_cast<ipp::Operation>(mock_response.operation_id()));
  CHECK(response) << "BUG: expected non-nullptr `ipp::Response`";

  response->StatusCode() = static_cast<ipp::Status>(mock_response.status());

  // TODO(b/178649877): libipp only offers version number manipulation
  // at the level of either the `Client` or the `Server`. We can't
  // do anything here on a per-response basis, but we should make a
  // note of how this interacts with the version number specified in
  // the Protobufs.
  for (const auto& mock_group : mock_response.groups()) {
    CopyGroup(mock_group, response.get());
  }
  return response;
}

}  // namespace mock_printer
