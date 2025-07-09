// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "proto_to_libipp.h"

#include <string>
#include <vector>
#include <cstddef>

#include <base/check.h>
#include <base/check_op.h>
#include <base/notreached.h>
#include <base/containers/fixed_flat_map.h>
#include <chromeos/libipp/ipp_enums.h>

namespace mock_printer {

namespace {

void CopyAttributes(const mocking::IppCollection& mock_collection,
                    ipp::Collection& collection);

// Returns the integer values from `mock_attribute`.
std::vector<int32_t> CopyInts(const mocking::IppAttribute& mock_attribute) {
  std::vector<int32_t> values;
  values.reserve(mock_attribute.int_values().int_values_size());
  for (const int32_t mock_int : mock_attribute.int_values().int_values()) {
    values.push_back(mock_int);
  }
  return values;
}

// Returns the string values from `mock_attribute`.
std::vector<std::string> CopyStrings(
    const mocking::IppAttribute& mock_attribute) {
  std::vector<std::string> values;
  values.reserve(mock_attribute.string_values().string_values_size());
  for (const mocking::OctetString& mock_string :
       mock_attribute.string_values().string_values()) {
    values.push_back(mock_string.value());
  }
  return values;
}

// Returns the string-with-language values from `mock_attribute`.
std::vector<ipp::StringWithLanguage> CopyStringsWithLanguage(
    const mocking::IppAttribute& mock_attribute) {
  std::vector<ipp::StringWithLanguage> values;
  values.reserve(mock_attribute.string_values().string_values_size());
  for (const mocking::OctetString& mock_string :
       mock_attribute.string_values().string_values()) {
    values.push_back(
        ipp::StringWithLanguage(mock_string.language(), mock_string.value()));
  }
  return values;
}

// Returns the integer ranges from `mock_attribute`.
std::vector<ipp::RangeOfInteger> CopyIntRanges(
    const mocking::IppAttribute& mock_attribute) {
  std::vector<ipp::RangeOfInteger> values;
  values.reserve(mock_attribute.int_ranges().int_ranges_size());
  for (const mocking::IntegerRange& mock_range :
       mock_attribute.int_ranges().int_ranges()) {
    values.push_back(
        ipp::RangeOfInteger(mock_range.minimum(), mock_range.maximum()));
  }
  return values;
}

// Returns the resolutions from `mock_attribute`.
std::vector<ipp::Resolution> CopyResolutions(
    const mocking::IppAttribute& mock_attribute) {
  std::vector<ipp::Resolution> values;
  values.reserve(mock_attribute.resolutions().resolutions_size());
  for (const mocking::Resolution& mock_resolution :
       mock_attribute.resolutions().resolutions()) {
    values.push_back(ipp::Resolution(
        mock_resolution.xres(), mock_resolution.yres(),
        mock_resolution.units() == mocking::Resolution::DOTS_PER_CENTIMETER
            ? ipp::Resolution::Units::kDotsPerCentimeter
            : ipp::Resolution::Units::kDotsPerInch));
  }
  return values;
}

// Returns the datetime objects from `mock_attribute`.
std::vector<ipp::DateTime> CopyDateTimes(
    const mocking::IppAttribute& mock_attribute) {
  std::vector<ipp::DateTime> values;
  values.reserve(mock_attribute.date_times().date_times_size());
  for (const mocking::DateTime& mock_datetime :
       mock_attribute.date_times().date_times()) {
    values.push_back(ipp::DateTime{
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
        .UTC_minutes = static_cast<uint8_t>(mock_datetime.utc_minutes())});
  }
  return values;
}

// Iterates over `mock_collection` and adds all attributes to
// `collection`.
void CopyAttributes(const mocking::IppCollection& mock_collection,
                    ipp::Collection& collection) {
  for (const auto& mock_attribute : mock_collection.attributes()) {
    switch (mock_attribute.value_case()) {
      case mocking::IppAttribute::ValueCase::kCollections: {
        size_t n = mock_attribute.collections().collections().size();
        ipp::CollsView new_colls;
        collection.AddAttr(mock_attribute.name(), n, new_colls);
        for (size_t i = 0; i < n; ++i) {
          CopyAttributes(mock_attribute.collections().collections()[i],
                         new_colls[i]);
        }
      } break;
      case mocking::IppAttribute::ValueCase::kIntValues:
        collection.AddAttr(mock_attribute.name(), CopyInts(mock_attribute));
        break;
      case mocking::IppAttribute::ValueCase::kStringValues:
        if (mock_attribute.type() ==
                mocking::IppAttribute::TEXT_WITH_LANGUAGE ||
            mock_attribute.type() ==
                mocking::IppAttribute::NAME_WITH_LANGUAGE) {
          collection.AddAttr(mock_attribute.name(),
                             static_cast<ipp::ValueTag>(mock_attribute.type()),
                             CopyStringsWithLanguage(mock_attribute));
        } else {
          collection.AddAttr(mock_attribute.name(),
                             static_cast<ipp::ValueTag>(mock_attribute.type()),
                             CopyStrings(mock_attribute));
        }
        break;
      case mocking::IppAttribute::ValueCase::kIntRanges:
        collection.AddAttr(mock_attribute.name(),
                           CopyIntRanges(mock_attribute));
        break;
      case mocking::IppAttribute::ValueCase::kResolutions:
        collection.AddAttr(mock_attribute.name(),
                           CopyResolutions(mock_attribute));
        break;
      case mocking::IppAttribute::ValueCase::kDateTimes:
        collection.AddAttr(mock_attribute.name(),
                           CopyDateTimes(mock_attribute));
        break;
      default:
        NOTREACHED() << "BUG: unhandled `mocking::IppAttribute` value case";
    }
  }
}

// Populates `response` with the contents of `mock_group`.
void CopyGroup(const mocking::IppGroup& mock_group, ipp::Frame& response) {
  const auto tag = mock_group.tag();

  CHECK_GE(tag, 0) << "group tag " << tag << " < 0";
  CHECK_LE(tag, 0x0f) << "group tag " << tag << " > 0x0f";

  ipp::CollsView::iterator group;
  while (response.Groups(static_cast<ipp::GroupTag>(tag)).size() <
         mock_group.collections().size())
    response.AddGroup(static_cast<ipp::GroupTag>(tag), group);
  group = response.Groups(static_cast<ipp::GroupTag>(tag)).begin();
  for (const auto& mock_collection : mock_group.collections()) {
    CopyAttributes(mock_collection, *group);
    ++group;
  }
}

}  // namespace

std::unique_ptr<ipp::Frame> ToResponse(
    const mocking::IppMessage& mock_response) {
  if (mock_response.operation_id() == 0) {
    // This happens when `mock_response` is unset, which is possible because it
    // is an optional field.
    return nullptr;
  }

  auto response = std::make_unique<ipp::Frame>(
      static_cast<ipp::Status>(mock_response.status()));

  // TODO(b/178649877): libipp only offers version number manipulation
  // at the level of either the `Client` or the `Server`. We can't
  // do anything here on a per-response basis, but we should make a
  // note of how this interacts with the version number specified in
  // the Protobufs.
  for (const auto& mock_group : mock_response.groups()) {
    CopyGroup(mock_group, *response.get());
  }
  return response;
}

}  // namespace mock_printer
