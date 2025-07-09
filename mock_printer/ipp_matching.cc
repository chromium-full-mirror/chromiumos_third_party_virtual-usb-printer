// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_matching.h"

#include <cstdint>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include <base/check.h>
#include <base/notreached.h>
#include <chromeos/libipp/attribute.h>
#include <chromeos/libipp/frame.h>

#include "mock_printer/proto/ipp.pb.h"

// By convention, helper functions that determine whether or not X
// matches Y actually return
// *  true if X _could_ match Y and
// *  false if X definitely does not match Y.
//
// This is relevant because an unset field in a mocking Protobuf is a
// trivial match. All that can be said is that the unset field does not
// immediately terminate our matching process. There are many fields to
// match to satisfy the public `IppMessageMatches()` function.

namespace mock_printer {
namespace {

bool IppCollectionMatches(const ipp::Collection& collection,
                          const mocking::IppCollection& mock_collection);

bool ResolutionMatches(const ipp::Resolution& resolution,
                       const mocking::Resolution& mock_resolution) {
  if ((resolution.units == ipp::Resolution::Units::kDotsPerInch &&
       mock_resolution.units() != mocking::Resolution::DOTS_PER_INCH) ||
      (resolution.units == ipp::Resolution::Units::kDotsPerCentimeter &&
       mock_resolution.units() != mocking::Resolution::DOTS_PER_CENTIMETER)) {
    return false;
  }
  return resolution.xres == mock_resolution.xres() &&
         resolution.yres == mock_resolution.yres();
}

// Compares corresponding members between a `mocking::DateTime` and
// an `ipp::DateTime`. Returns whether the members match.
bool MockDateTimeMemberMatches(int32_t member, int32_t mock_member) {
  return !mock_member || mock_member == member;
}

// Overload for the above specific to the `ipp::DateTime::UTC_direction`
// member.
bool MockDateTimeMemberMatches(int32_t direction,
                               mocking::DateTime::UtcDirection mock_direction) {
  switch (mock_direction) {
    case mocking::DateTime::PLUS:
      return direction == '+';
    case mocking::DateTime::MINUS:
      return direction == '-';
    // UNSPECIFIED shall always be taken to match.
    default:
      return true;
  }
}

bool DateTimeMatches(const ipp::DateTime& date_time,
                     const mocking::DateTime& mock_date_time) {
  return MockDateTimeMemberMatches(date_time.year, mock_date_time.year()) &&
         MockDateTimeMemberMatches(date_time.month, mock_date_time.month()) &&
         MockDateTimeMemberMatches(date_time.day, mock_date_time.day()) &&
         MockDateTimeMemberMatches(date_time.hour, mock_date_time.hour()) &&
         MockDateTimeMemberMatches(date_time.minutes,
                                   mock_date_time.minutes()) &&
         MockDateTimeMemberMatches(date_time.seconds,
                                   mock_date_time.seconds()) &&
         MockDateTimeMemberMatches(date_time.deci_seconds,
                                   mock_date_time.deciseconds()) &&
         MockDateTimeMemberMatches(date_time.UTC_direction,
                                   mock_date_time.utc_direction()) &&
         MockDateTimeMemberMatches(date_time.UTC_hours,
                                   mock_date_time.utc_hours()) &&
         MockDateTimeMemberMatches(date_time.UTC_minutes,
                                   mock_date_time.utc_minutes());
}

bool EmbeddedCollectionsMatch(const ipp::Attribute& attribute,
                              const mocking::IppAttribute& mock_attribute) {
  if (mock_attribute.collections().collections_size() != attribute.Size()) {
    return false;
  }
  std::size_t index = 0;
  for (const mocking::IppCollection& mock_collection :
       mock_attribute.collections().collections()) {
    const ipp::Collection& collection = attribute.Colls()[index];
    if (!IppCollectionMatches(collection, mock_collection)) {
      return false;
    }
    index++;
  }
  return true;
}

bool IntValuesMatch(const ipp::Attribute& attribute,
                    const mocking::IppAttribute& mock_attribute) {
  std::vector<int32_t> values;
  attribute.GetValues(values);
  if (mock_attribute.int_values().int_values_size() != values.size()) {
    return false;
  }
  std::size_t index = 0;
  for (const int mock_integer : mock_attribute.int_values().int_values()) {
    if (values[index] != mock_integer) {
      return false;
    }
    index++;
  }
  return true;
}

bool StringValuesMatch(const ipp::Attribute& attribute,
                       const mocking::IppAttribute& mock_attribute) {
  // We allow string-coercible types to pass onto actual matching.
  // We bail out here if it's not a string-ish type.
  switch (attribute.Tag()) {
    case (ipp::ValueTag::textWithoutLanguage):
    case (ipp::ValueTag::nameWithoutLanguage):
    case (ipp::ValueTag::keyword):
    case (ipp::ValueTag::uri):
    case (ipp::ValueTag::uriScheme):
    case (ipp::ValueTag::charset):
    case (ipp::ValueTag::naturalLanguage):
    case (ipp::ValueTag::mimeMediaType):
      break;
    default:
      return false;
  }
  std::vector<std::string> values;
  attribute.GetValues(values);
  if (mock_attribute.string_values().string_values_size() != values.size()) {
    return false;
  }

  std::size_t index = 0;
  for (const mocking::OctetString& octet_string :
       mock_attribute.string_values().string_values()) {
    if (!octet_string.language().empty() ||
        values[index] != octet_string.value()) {
      return false;
    }
    index++;
  }
  return true;
}

bool IntRangesMatch(const ipp::Attribute& attribute,
                    const mocking::IppAttribute& mock_attribute) {
  std::vector<ipp::RangeOfInteger> values;
  attribute.GetValues(values);
  if (values.size() != mock_attribute.int_ranges().int_ranges_size()) {
    return false;
  }
  std::size_t index = 0;
  for (const mocking::IntegerRange& mock_range :
       mock_attribute.int_ranges().int_ranges()) {
    if (values[index].min_value != mock_range.minimum() ||
        values[index].max_value != mock_range.maximum()) {
      return false;
    }
    index++;
  }
  return true;
}

bool ResolutionsMatch(const ipp::Attribute& attribute,
                      const mocking::IppAttribute& mock_attribute) {
  std::vector<ipp::Resolution> values;
  attribute.GetValues(values);
  if (values.size() != mock_attribute.resolutions().resolutions_size()) {
    return false;
  }
  std::size_t index = 0;
  for (const mocking::Resolution& mock_resolution :
       mock_attribute.resolutions().resolutions()) {
    if (!ResolutionMatches(values[index], mock_resolution)) {
      return false;
    }
    index++;
  }
  return true;
}

bool DateTimesMatch(const ipp::Attribute& attribute,
                    const mocking::IppAttribute& mock_attribute) {
  std::vector<ipp::DateTime> values;
  attribute.GetValues(values);
  if (values.size() != mock_attribute.date_times().date_times_size()) {
    return false;
  }
  std::size_t index = 0;
  for (const mocking::DateTime& mock_date_time :
       mock_attribute.date_times().date_times()) {
    if (!DateTimeMatches(values[index], mock_date_time)) {
      return false;
    }
    index++;
  }
  return true;
}

bool AttributeTypeMatches(const ipp::Attribute& attribute,
                          const mocking::IppAttribute& mock_attribute) {
  if (mock_attribute.type() == mocking::IppAttribute::UNSPECIFIED) {
    return true;
  }
  switch (attribute.Tag()) {
    case (ipp::ValueTag::integer):
      return mock_attribute.type() == mocking::IppAttribute::INTEGER;
    case (ipp::ValueTag::boolean):
      return mock_attribute.type() == mocking::IppAttribute::BOOLEAN;
    case (ipp::ValueTag::enum_):
      return mock_attribute.type() == mocking::IppAttribute::ENUM;
    case (ipp::ValueTag::octetString):
      return mock_attribute.type() == mocking::IppAttribute::OCTET_STRING;
    case (ipp::ValueTag::dateTime):
      return mock_attribute.type() == mocking::IppAttribute::DATETIME;
    case (ipp::ValueTag::resolution):
      return mock_attribute.type() == mocking::IppAttribute::RESOLUTION;
    case (ipp::ValueTag::rangeOfInteger):
      return mock_attribute.type() == mocking::IppAttribute::RANGE_OF_INTEGER;
    case (ipp::ValueTag::collection):
      return mock_attribute.type() == mocking::IppAttribute::BEGIN_COLLECTION;
    case (ipp::ValueTag::textWithLanguage):
      return mock_attribute.type() == mocking::IppAttribute::TEXT_WITH_LANGUAGE;
    case (ipp::ValueTag::textWithoutLanguage):
      return mock_attribute.type() ==
             mocking::IppAttribute::TEXT_WITHOUT_LANGUAGE;
    case (ipp::ValueTag::nameWithLanguage):
      return mock_attribute.type() == mocking::IppAttribute::NAME_WITH_LANGUAGE;
    case (ipp::ValueTag::nameWithoutLanguage):
      return mock_attribute.type() ==
             mocking::IppAttribute::NAME_WITHOUT_LANGUAGE;
    case (ipp::ValueTag::keyword):
      return mock_attribute.type() == mocking::IppAttribute::KEYWORD;
    case (ipp::ValueTag::uri):
      return mock_attribute.type() == mocking::IppAttribute::URI;
    case (ipp::ValueTag::uriScheme):
      return mock_attribute.type() == mocking::IppAttribute::URI_SCHEME;
    case (ipp::ValueTag::charset):
      return mock_attribute.type() == mocking::IppAttribute::CHARSET;
    case (ipp::ValueTag::naturalLanguage):
      return mock_attribute.type() == mocking::IppAttribute::NATURAL_LANGUAGE;
    case (ipp::ValueTag::mimeMediaType):
      return mock_attribute.type() == mocking::IppAttribute::MIME_MEDIA_TYPE;
    default:
      NOTREACHED() << "BUG: unhandled `ipp::ValueTag`";
  }
}

bool AttributeValueMatches(const ipp::Attribute& attribute,
                           const mocking::IppAttribute& mock_attribute) {
  switch (mock_attribute.value_case()) {
    case (mocking::IppAttribute::ValueCase::VALUE_NOT_SET):
      return true;
    case (mocking::IppAttribute::ValueCase::kCollections):
      return EmbeddedCollectionsMatch(attribute, mock_attribute);
    case (mocking::IppAttribute::ValueCase::kIntValues):
      return IntValuesMatch(attribute, mock_attribute);
    case (mocking::IppAttribute::ValueCase::kStringValues):
      return StringValuesMatch(attribute, mock_attribute);
    case (mocking::IppAttribute::ValueCase::kIntRanges):
      return IntRangesMatch(attribute, mock_attribute);
    case (mocking::IppAttribute::ValueCase::kDateTimes):
      return DateTimesMatch(attribute, mock_attribute);
    case (mocking::IppAttribute::ValueCase::kResolutions):
      return ResolutionsMatch(attribute, mock_attribute);
    default:
      NOTREACHED() << "BUG: unhandled `ValueCase`";
  }
}

bool IppAttributeMatches(const ipp::Attribute& attribute,
                         const mocking::IppAttribute& mock_attribute) {
  return AttributeTypeMatches(attribute, mock_attribute) &&
         AttributeValueMatches(attribute, mock_attribute);
}

bool IppCollectionMatches(const ipp::Collection& collection,
                          const mocking::IppCollection& mock_collection) {
  if (collection.size() != mock_collection.attributes_size()) {
    return false;
  }

  for (const mocking::IppAttribute& mock_attribute :
       mock_collection.attributes()) {
    ipp::Collection::const_iterator it_attr =
        collection.GetAttr(mock_attribute.name());
    if (it_attr == collection.end() ||
        !IppAttributeMatches(*it_attr, mock_attribute)) {
      return false;
    }
  }
  return true;
}

ipp::GroupTag TagFromProtobuf(const mocking::IppGroup& mock_group) {
  switch (mock_group.tag()) {
    case (mocking::IppGroup::DOCUMENT_ATTRIBUTES):
      return ipp::GroupTag::document_attributes;
    case (mocking::IppGroup::EVENT_NOTIFICATION_ATTRIBUTES):
      return ipp::GroupTag::event_notification_attributes;
    case (mocking::IppGroup::JOB_ATTRIBUTES):
      return ipp::GroupTag::job_attributes;
    case (mocking::IppGroup::OPERATION_ATTRIBUTES):
      return ipp::GroupTag::operation_attributes;
    case (mocking::IppGroup::PRINTER_ATTRIBUTES):
      return ipp::GroupTag::printer_attributes;
    case (mocking::IppGroup::RESOURCE_ATTRIBUTES):
      return ipp::GroupTag::resource_attributes;
    case (mocking::IppGroup::SUBSCRIPTION_ATTRIBUTES):
      return ipp::GroupTag::subscription_attributes;
    case (mocking::IppGroup::SYSTEM_ATTRIBUTES):
      return ipp::GroupTag::system_attributes;
    case (mocking::IppGroup::UNSUPPORTED_ATTRIBUTES):
      return ipp::GroupTag::unsupported_attributes;
    default:
      NOTREACHED() << "BUG: unhandled IppGroupTag variant";
  }
}

bool IppGroupMatches(ipp::ConstCollsView group,
                     const mocking::IppGroup& mock_group) {
  // Collections are optional.
  if (!mock_group.collections_size()) {
    return true;
  }

  // ... but if collections are specified, they need to match exactly.
  if (group.size() != mock_group.collections_size()) {
    return false;
  }

  std::size_t collection_index = 0;
  for (const auto& mock_collection : mock_group.collections()) {
    if (!IppCollectionMatches(group[collection_index], mock_collection)) {
      return false;
    }
    collection_index++;
  }
  return true;
}

}  // namespace

bool IppMessageMatches(const ipp::Frame& message,
                       const mocking::IppMessage& mock_message) {
  if (static_cast<std::underlying_type_t<ipp::Operation>>(
          message.OperationId()) != mock_message.operation_id()) {
    return false;
  }

  // Save all GroupTags that have at least one group (collection).
  std::set<ipp::GroupTag> group_tags_to_match;
  for (const ipp::GroupTag tag : ipp::kGroupTags) {
    if (!message.Groups(tag).empty()) {
      group_tags_to_match.insert(tag);
    }
  }

  for (const auto& mock_group : mock_message.groups()) {
    const ipp::GroupTag tag = TagFromProtobuf(mock_group);
    group_tags_to_match.erase(tag);
    if (!IppGroupMatches(message.Groups(tag), mock_group)) {
      return false;
    }
  }

  // We reach this point <=> all groups from `mock_message` were matched
  // successfully. Now, we have to only check if all groups from `message` were
  // matched.
  return group_tags_to_match.empty();
}

}  // namespace mock_printer
