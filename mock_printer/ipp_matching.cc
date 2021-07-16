// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_matching.h"

#include <string>
#include <type_traits>

#include <base/check.h>
#include <chromeos/libipp/ipp.h>

#include "proto/ipp.pb.h"

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

bool IppCollectionMatches(const ipp::Collection* const collection,
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

bool EmbeddedCollectionsMatch(const ipp::Attribute* const attribute,
                              const mocking::IppAttribute& mock_attribute) {
  if (attribute->GetType() != ipp::AttrType::collection ||
      mock_attribute.collections_size() != attribute->GetSize()) {
    return false;
  }
  std::size_t index = 0;
  for (const mocking::IppCollection& mock_collection :
       mock_attribute.collections()) {
    const ipp::Collection* const collection = attribute->GetCollection(index);
    CHECK(collection) << "BUG: expected non-nullptr ipp::Collection*";
    if (!IppCollectionMatches(collection, mock_collection)) {
      return false;
    }
    index++;
  }
  return true;
}

bool IntValuesMatch(const ipp::Attribute* const attribute,
                    const mocking::IppAttribute& mock_attribute) {
  if (attribute->GetType() != ipp::AttrType::integer ||
      mock_attribute.int_values_size() != attribute->GetSize()) {
    return false;
  }
  std::size_t index = 0;
  for (const int mock_integer : mock_attribute.int_values()) {
    int true_integer;
    CHECK(attribute->GetValue(&true_integer, index))
        << "BUG: expected integer here";
    if (true_integer != mock_integer) {
      return false;
    }
    index++;
  }
  return true;
}

bool StringValuesMatch(const ipp::Attribute* const attribute,
                       const mocking::IppAttribute& mock_attribute) {
  // We allow string-coercible types to pass onto actual matching.
  // We bail out here if it's not a string-ish type.
  switch (attribute->GetType()) {
    case (ipp::AttrType::text):
    case (ipp::AttrType::name):
    case (ipp::AttrType::keyword):
    case (ipp::AttrType::uri):
    case (ipp::AttrType::uriScheme):
    case (ipp::AttrType::charset):
    case (ipp::AttrType::naturalLanguage):
    case (ipp::AttrType::mimeMediaType):
      break;
    default:
      return false;
  }
  if (mock_attribute.string_values_size() != attribute->GetSize()) {
    return false;
  }

  std::size_t index = 0;
  for (const std::string& mock_string : mock_attribute.string_values()) {
    std::string true_string;
    CHECK(attribute->GetValue(&true_string, index))
        << "BUG: expected string here";
    if (true_string != mock_string) {
      return false;
    }
    index++;
  }
  return true;
}

bool IntRangesMatch(const ipp::Attribute* const attribute,
                    const mocking::IppAttribute& mock_attribute) {
  if (attribute->GetType() != ipp::AttrType::rangeOfInteger ||
      attribute->GetSize() != mock_attribute.int_ranges_size()) {
    return false;
  }
  std::size_t index = 0;
  for (const mocking::IntegerRange& mock_range : mock_attribute.int_ranges()) {
    ipp::RangeOfInteger true_range;
    CHECK(attribute->GetValue(&true_range, index))
        << "BUG: expected RangeOfInteger here";
    if (true_range.min_value != mock_range.minimum() ||
        true_range.max_value != mock_range.maximum()) {
      return false;
    }
    index++;
  }
  return true;
}

bool ResolutionsMatch(const ipp::Attribute* const attribute,
                      const mocking::IppAttribute& mock_attribute) {
  if (attribute->GetType() != ipp::AttrType::resolution ||
      attribute->GetSize() != mock_attribute.resolutions_size()) {
    return false;
  }
  std::size_t index = 0;
  for (const mocking::Resolution& mock_resolution :
       mock_attribute.resolutions()) {
    ipp::Resolution true_resolution;
    CHECK(attribute->GetValue(&true_resolution, index))
        << "BUG: expected Resolution here";
    if (!ResolutionMatches(true_resolution, mock_resolution)) {
      return false;
    }
    index++;
  }
  return true;
}

bool IppAttributeMatches(const ipp::Attribute* const attribute,
                         const mocking::IppAttribute& mock_attribute) {
  // Validation code will already have checked that `mock_attribute`
  // contains at most one populated field. If any is specified, then
  // it must match the corresponding value in `attribute`.
  if (mock_attribute.collections_size()) {
    return EmbeddedCollectionsMatch(attribute, mock_attribute);
  } else if (mock_attribute.int_values_size()) {
    return IntValuesMatch(attribute, mock_attribute);
  } else if (mock_attribute.string_values_size()) {
    return StringValuesMatch(attribute, mock_attribute);
  } else if (mock_attribute.int_ranges_size()) {
    return IntRangesMatch(attribute, mock_attribute);
  } else if (mock_attribute.resolutions_size()) {
    return ResolutionsMatch(attribute, mock_attribute);
  }

  // "At most one populated field" implies that the matcher Protobuf
  // doesn't _need_ to specify attribute values at all. If none are
  // given, it's a trivial match.
  return true;
}

bool IppCollectionMatches(const ipp::Collection* const collection,
                          const mocking::IppCollection& mock_collection) {
  if (collection->GetAllAttributes().size() !=
      mock_collection.attributes_size()) {
    return false;
  }

  for (const mocking::IppAttribute& mock_attribute :
       mock_collection.attributes()) {
    const ipp::Attribute* const attribute =
        collection->GetAttribute(mock_attribute.name());
    if (!attribute || !IppAttributeMatches(attribute, mock_attribute)) {
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
      CHECK(false) << "BUG: unhandled IppGroupTag variant";
      // Silences compiler; never reached
      return ipp::GroupTag::unsupported_attributes;
  }
}

bool IppGroupMatches(const ipp::Group* const group,
                     const mocking::IppGroup& mock_group) {
  // Collections are optional.
  if (!mock_group.collections_size()) {
    return true;
  }

  // ... but if collections are specified, they need to match exactly.
  if (group->GetSize() != mock_group.collections_size()) {
    return false;
  }

  std::size_t collection_index = 0;
  for (const auto& mock_collection : mock_group.collections()) {
    const ipp::Collection* const collection =
        group->GetCollection(collection_index);
    if (!collection) {
      return false;
    }
    if (!IppCollectionMatches(collection, mock_collection)) {
      return false;
    }
    collection_index++;
  }
  return true;
}

}  // namespace

bool IppMessageMatches(const ipp::Request* const message,
                       const mocking::IppMessage& mock_message) {
  if (static_cast<std::underlying_type_t<ipp::Operation>>(
          message->GetOperationId()) != mock_message.operation_id()) {
    return false;
  }

  // Groups must be written out in full to match. If the number of
  // groups doesn't match, then that definitely negates any chance at a
  // match overall.
  const size_t num_groups = message->GetAllGroups().size();
  if (num_groups != mock_message.groups_size()) {
    return false;
  }

  for (const auto& mock_group : mock_message.groups()) {
    const ipp::Group* const group =
        message->GetGroup(TagFromProtobuf(mock_group));
    if (!group) {
      return false;
    }
    if (!IppGroupMatches(group, mock_group)) {
      return false;
    }
  }
  return true;
}

}  // namespace mock_printer
