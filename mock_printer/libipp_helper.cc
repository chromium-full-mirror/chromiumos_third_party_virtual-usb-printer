// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libipp_helper.h"

#include <cstddef>

#include <base/logging.h>
#include <base/strings/string_piece.h>
#include <base/strings/stringprintf.h>
#include <base/format_macros.h>

namespace mock_printer {

namespace {

constexpr base::StringPiece kUnknownValue = "        [unknown value]";

// Adds the string representation of the values in `attribute` to
// `lines`.
//
// Attribute values are indented four times.
template <typename T>
void AddAttributeValues(const ipp::Attribute* attribute,
                        std::vector<std::string>* lines) {
  for (size_t i = 0; i < attribute->Size(); ++i) {
    T value;
    if (attribute->GetValue(&value, i)) {
      lines->push_back(
          base::StringPrintf("        %s", ipp::ToString(value).c_str()));
    } else {
      lines->push_back(std::string(kUnknownValue));
    }
  }
}

// Hand-rolled special-case handling for `std::string`, for which we
// have no need (and no way) to call `ipp::ToString()`.
void AddAttributeStringValues(const ipp::Attribute* attribute,
                              std::vector<std::string>* lines) {
  for (size_t i = 0; i < attribute->Size(); ++i) {
    std::string value;
    if (attribute->GetValue(&value, i)) {
      lines->push_back(base::StringPrintf("        %s", value.c_str()));
    } else {
      lines->push_back(std::string(kUnknownValue));
    }
  }
}

// Hand-rolled special-case handling for `bool`, for which we cannot use
// the simple template.
void AddAttributeBooleanValues(const ipp::Attribute* attribute,
                               std::vector<std::string>* lines) {
  for (size_t i = 0; i < attribute->Size(); ++i) {
    // libipp uses an `int` to store the boolean type.
    int value;
    if (attribute->GetValue(&value, i)) {
      // The explicit initialization of `bool` is needed to use the
      // appropriate `ipp::ToString()` override.
      lines->push_back(base::StringPrintf(
          "        %s", ipp::ToString(static_cast<bool>(value)).c_str()));
    } else {
      lines->push_back(std::string(kUnknownValue));
    }
  }
}

// Adds the string representation of `attribute` to `lines`.
// Note that some attribute types (e.g. embedded collections) are
// unsupported and are represented with a placeholder.
//
// Attributes are indented thrice and their contents four times.
void AddAttribute(const ipp::Attribute* attribute,
                  std::vector<std::string>* lines) {
  lines->push_back(
      base::StringPrintf("      Attribute ``%s'' (%s, %" PRIuS " values)",
                         std::string(attribute->Name()).c_str(),
                         std::string(ipp::ToStrView(attribute->Tag())).c_str(),
                         attribute->Size()));
  if (!attribute->Size()) {
    return;
  }

  switch (attribute->Tag()) {
    case ipp::ValueTag::integer:
    case ipp::ValueTag::enum_:
      return AddAttributeValues<int>(attribute, lines);
    case ipp::ValueTag::boolean:
      return AddAttributeBooleanValues(attribute, lines);
    case ipp::ValueTag::octetString:
    case ipp::ValueTag::textWithLanguage:
    case ipp::ValueTag::textWithoutLanguage:
    case ipp::ValueTag::nameWithLanguage:
    case ipp::ValueTag::nameWithoutLanguage:
    case ipp::ValueTag::keyword:
    case ipp::ValueTag::uri:
    case ipp::ValueTag::uriScheme:
    case ipp::ValueTag::charset:
    case ipp::ValueTag::naturalLanguage:
    case ipp::ValueTag::mimeMediaType:
      return AddAttributeStringValues(attribute, lines);
    case ipp::ValueTag::dateTime:
      return AddAttributeValues<ipp::DateTime>(attribute, lines);
    case ipp::ValueTag::resolution:
      return AddAttributeValues<ipp::Resolution>(attribute, lines);
    case ipp::ValueTag::rangeOfInteger:
      return AddAttributeValues<ipp::RangeOfInteger>(attribute, lines);
    default:
      lines->push_back("        [unsupported type]");
      break;
  }
}

// Adds the string representation of `collection` to `lines`.
//
// Collections are indented twice.
void AddCollection(const ipp::Collection* collection,
                   const size_t collection_index,
                   std::vector<std::string>* lines) {
  lines->push_back(base::StringPrintf(
      "    Collection %" PRIuS " (%" PRIuS " attributes)", collection_index,
      collection->GetAllAttributes().size()));
  for (const ipp::Attribute* attribute : collection->GetAllAttributes()) {
    AddAttribute(attribute, lines);
  }
}

// Adds the string representation of `group` to `lines`.
//
// Groups are indented once.
void AddGroup(const ipp::GroupTag tag,
              const std::vector<const ipp::Collection*>& groups,
              std::vector<std::string>* lines) {
  if (groups.empty()) {
    return;
  }
  lines->push_back(
      base::StringPrintf("  Group (tag: 0x%04x; collections: %" PRIuS ")",
                         static_cast<unsigned int>(tag), groups.size()));
  for (size_t i = 0; i < groups.size(); ++i) {
    AddCollection(groups[i], i, lines);
  }
}

}  // namespace

void MultilineLogHolder::LogError(base::StringPiece preface) const {
  if (lines.empty()) {
    return;
  }
  if (!preface.empty()) {
    LOG(ERROR) << preface;
  }
  for (const std::string& line : lines) {
    LOG(ERROR) << line;
  }
}

void MultilineLogHolder::Vlog(int level, base::StringPiece preface) const {
  if (lines.empty()) {
    return;
  }
  if (!preface.empty()) {
    VLOG(level) << preface;
  }
  for (const std::string& line : lines) {
    VLOG(level) << line;
  }
}

MultilineLogHolder ToString(const ipp::Frame* request) {
  std::vector<std::string> lines;
  size_t groups_count = 0;
  for (ipp::GroupTag tag : ipp::kGroupTags) {
    groups_count += request->GetGroups(tag).size();
  }
  lines.push_back(base::StringPrintf(
      "Request (operation ID: 0x%04x; groups: %" PRIuS ")",
      static_cast<unsigned int>(request->OperationId()), groups_count));
  for (ipp::GroupTag tag : ipp::kGroupTags) {
    AddGroup(tag, request->GetGroups(tag), &lines);
  }
  return MultilineLogHolder{.lines = lines};
}

}  // namespace mock_printer
