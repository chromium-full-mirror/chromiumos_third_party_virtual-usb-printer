// Copyright 2019 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_util.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <base/json/json_reader.h>
#include <base/strings/stringprintf.h>
#include <base/values.h>

#include "cups_constants.h"
#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/common/value_util.h"

namespace {

using std::string_literals::operator""s;

const std::vector<uint8_t> CreateByteVector(const std::string& str) {
  std::vector<uint8_t> v(str.size());
  for (size_t i = 0; i < str.size(); ++i) {
    v[i] = static_cast<uint8_t>(str[i]);
  }
  return v;
}

TEST(IppAttributeEquality, SameAttributes) {
  std::optional<base::Value> val = GetJSONValue("123");
  IppAttribute value1("integer", "test-attribute", &(val.value()));
  IppAttribute value2("integer", "test-attribute", &(val.value()));
  EXPECT_EQ(value1, value2);
}

TEST(IppAttributeEquality, DifferentTypes) {
  std::optional<base::Value> val = GetJSONValue("123");
  IppAttribute value1("integer", "test-attribute", &(val.value()));
  IppAttribute value2("enum", "test-attribute", &(val.value()));
  EXPECT_NE(value1, value2);
}

TEST(IppAttributeEquality, DifferentNames) {
  std::optional<base::Value> val = GetJSONValue("123");
  IppAttribute value1("integer", "test-attribute", &(val.value()));
  IppAttribute value2("integer", "fake-attribute", &(val.value()));
  EXPECT_NE(value1, value2);
}

// TODO(valleau): Add a test case for inequality when the actual value members
// differ once chromelib is updated and the equality operator for base::Value is
// defined.

TEST(IppHeader, DeserializeValid) {
  std::vector<uint8_t> message = {0x02, 0x00, 0x00, 0x06, 0x00, 0x00,
                                  0x00, 0x07, 0x00, 0x01, 0x70, 0x29};
  SmartBuffer buf(message);
  std::optional<IppHeader> opt_ipp_header = IppHeader::Deserialize(&buf);
  EXPECT_TRUE(opt_ipp_header);
  IppHeader ipp_header = opt_ipp_header.value();

  EXPECT_EQ(ipp_header.major, 2);
  EXPECT_EQ(ipp_header.minor, 0);
  EXPECT_EQ(ipp_header.operation_id, 0x0006);
  EXPECT_EQ(ipp_header.request_id, 7);
}

TEST(IppHeader, DeserializeInvalid) {
  std::vector<uint8_t> message = {0x02, 0x00, 0x00, 0x06, 0x00, 0x00};
  SmartBuffer buf(message);
  EXPECT_FALSE(IppHeader::Deserialize(&buf));
}

TEST(IppHeader, Serialize) {
  IppHeader header;
  header.major = 2;
  header.minor = 0;
  header.operation_id = 0x0009;
  header.request_id = 0x22330077;

  SmartBuffer buf;
  header.Serialize(&buf);
  EXPECT_EQ(buf.size(), sizeof(IppHeader));
  std::vector<uint8_t> expected = {0x02, 0x00, 0x00, 0x09,
                                   0x22, 0x33, 0x00, 0x77};
  EXPECT_EQ(buf.contents(), expected);
}

TEST(RemoveAttributes, HasEndTag) {
  std::string message =
      // IPP attributes.
      "\x01\x47\x00\x12"
      "attributes-charset"
      "\x00\x05"
      "utf-8"
      "\x48\x00\x1b"
      "attributes-natural-language"
      "\x00\x05"
      "en-us"
      "\x45\x00\x0b"
      "printer-uri"
      "\x00\x19"
      "ipp://18d1_505e/ipp/print\x03"
      // Body
      "test image data"s;

  SmartBuffer buf;
  buf.Add(message);
  EXPECT_TRUE(RemoveIppAttributes(&buf));

  std::string body = "test image data";
  std::vector<uint8_t> expected(body.begin(), body.end());
  EXPECT_EQ(buf.contents(), expected);
}

TEST(RemoveAttributes, EndCharacterWithinAttributes) {
  std::string message =
      // IPP attributes.
      "\x01\x47\x00\x12"
      "attributes-charset"
      "\x00\x03"
      "utf\x03"s;

  SmartBuffer buf;
  buf.Add(message);
  EXPECT_TRUE(RemoveIppAttributes(&buf));
  EXPECT_EQ(buf.size(), 0);
}

TEST(RemoveAttributes, EmptyBody) {
  std::string message =
      // IPP attributes.
      "\x01\x47\x00\x12"
      "attributes-charset"
      "\x00\x05"
      "utf-8\x03"s;

  SmartBuffer buf;
  buf.Add(message);
  EXPECT_TRUE(RemoveIppAttributes(&buf));
  EXPECT_EQ(buf.size(), 0);
}

TEST(RemoveAttributes, MultipleEndTags) {
  std::string message =
      // IPP attributes.
      "\x01\x47\x00\x12"
      "attributes-charset"
      "\x00\x05"
      "utf-8\x03"
      "test mess\x03age"s;

  SmartBuffer buf;
  buf.Add(message);
  EXPECT_TRUE(RemoveIppAttributes(&buf));

  std::string body = "test mess\x03age";
  std::vector<uint8_t> expected(body.begin(), body.end());
  EXPECT_EQ(buf.contents(), expected);
}

TEST(RemoveAttributes, MultipleGroups) {
  std::string message =
      // IPP attributes.
      "\x01\x47\x00\x12"
      "attributes-charset"
      "\x00\x05"
      "utf-8"
      "\x05\x13\x00\x15"
      "other-attribute-array"
      "\x00\x05"
      "first"
      "\x13\x00\x00"
      "\x00\x06"
      "second\x03"
      "test message"s;

  SmartBuffer buf;
  buf.Add(message);
  EXPECT_TRUE(RemoveIppAttributes(&buf));

  std::string body = "test message";
  std::vector<uint8_t> expected(body.begin(), body.end());
  EXPECT_EQ(buf.contents(), expected);
}

TEST(RemoveAttributes, NoEndTag) {
  std::string message =
      // IPP attributes.
      "\x01\x47\x00\x12"
      "attributes-charset"
      "\x00\x05"
      "utf-8"
      "test image data"s;

  SmartBuffer buf;
  buf.Add(message);
  EXPECT_FALSE(RemoveIppAttributes(&buf));
}

TEST(RemoveAttributes, InvalidAttributeSize) {
  std::string message =
      // IPP attributes.
      "\x01\x47\x00\x12"
      "attributes-charset"
      "\x00\x06"
      "utf-8"s;

  SmartBuffer buf;
  buf.Add(message);
  EXPECT_FALSE(RemoveIppAttributes(&buf));
}

TEST(GetIppTag, ValidTagName) {
  EXPECT_EQ(GetIppTag(kInteger), IppTag::INTEGER);
  EXPECT_EQ(GetIppTag(kDateTime), IppTag::DATE);

  std::string tagString(1, static_cast<uint8_t>(IppTag::INTEGER));
  std::ostringstream oss;
  oss << IppTag::INTEGER;
  EXPECT_EQ(oss.str(), tagString);
}

TEST(GetIppTag, InvalidTagName) {
  EXPECT_DEATH(GetIppTag("InvalidName"), "Given unknown tag name");
}

TEST(GetAttribute, ValidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "integer",
      "name": "printer-config-change-time",
      "value": 1834787
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  std::optional<base::Value> actual_value = GetJSONValue("1834787");
  IppAttribute expected("integer", "printer-config-change-time",
                        &(actual_value.value()));
  EXPECT_EQ(GetAttribute(value->GetDict()), expected);
}

TEST(GetAttribute, InvalidAttributes) {
  const std::string json_contents1 = R"(
    {
      "type": 123,
      "name": "printer-config-change-time",
      "value": ""
    }
  )";
  std::optional<base::Value> value1 = GetJSONValue(json_contents1);
  EXPECT_DEATH(GetAttribute(value1->GetDict()), "Failed to retrieve type");

  const std::string json_contents2 = R"(
    {
      "type": "keyword",
      "name": [ "hello" ],
      "value": ""
    }
  )";
  std::optional<base::Value> value2 = GetJSONValue(json_contents2);
  EXPECT_DEATH(GetAttribute(value2->GetDict()), "Failed to retrieve name");
}

TEST(GetAttributes, ValidAttributes) {
  const std::string operation_attributes_json = R"(
    {
      "operation_attributes": [{
        "type": "charset",
        "name": "attributes-charset",
        "value": "utf-8"
      }, {
        "type": "naturalLanguage",
        "name": "attributes-natural-language",
        "value": "en-us"
      }]
    }
  )";
  std::optional<base::Value> operation_attributes_value =
      GetJSONValue(operation_attributes_json);

  std::optional<base::Value> actual_value1 = GetJSONValue("\"utf-8\"");
  std::optional<base::Value> actual_value2 = GetJSONValue("\"en-us\"");
  std::vector<IppAttribute> expected = {
      IppAttribute("charset", "attributes-charset", &(actual_value1.value())),
      IppAttribute("naturalLanguage", "attributes-natural-language",
                   &(actual_value2.value()))};

  std::vector<IppAttribute> actual = GetAttributes(
      operation_attributes_value->GetDict(), "operation_attributes");

  EXPECT_EQ(actual, expected);
}

TEST(GetAttributes, InvalidAttributes) {
  // We expect this case to fail because the there is no attributes list for the
  // provided key.
  const std::string json_contents1 = R"(
    {
      "printer_attributes": [
        { "type": "charset", "name": "charset-configured", "value": "utf-8" }
      ]
    }
  )";
  std::optional<base::Value> value1 = GetJSONValue(json_contents1);
  EXPECT_DEATH(GetAttributes(value1->GetDict(), "operation_attributes"),
               "Failed to extract attributes list for key");

  const std::string json_contents2 = R"(
    { "printer_attributes": "not a list" }
  )";
  std::optional<base::Value> value2 = GetJSONValue(json_contents2);
  EXPECT_DEATH(GetAttributes(value2->GetDict(), "printer_attributes"),
               "Failed to extract attributes list for key");
}

TEST(IppAttributeGetBool, ValidAttributes) {
  const std::string json_contents1 = R"(
    { "type": "boolean", "name": "color-supported", "value": false }
  )";
  std::optional<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1->GetDict());
  EXPECT_EQ(attribute1.GetBool(), false);

  const std::string json_contents2 = R"(
    { "type": "boolean", "name": "color-supported", "value": true }
  )";
  std::optional<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2->GetDict());
  EXPECT_EQ(attribute2.GetBool(), true);
}

// In this test we expect crashes because the "value" fields in the attributes
// are invalid.
TEST(IppAttributeGetBool, InvalidAttributes) {
  const std::string json_contents1 = R"(
    { "type": "boolean", "name": "color-supported", "value": 0 }
  )";
  std::optional<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1->GetDict());
  EXPECT_DEATH(attribute1.GetBool(), "Failed to retrieve boolean");

  const std::string json_contents2 = R"(
    { "type": "boolean", "name": "color-supported", "value": 0 }
  )";
  std::optional<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2->GetDict());
  EXPECT_DEATH(attribute2.GetBool(), "Failed to retrieve boolean");
}

TEST(IppAttributeGetBools, ValidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "boolean",
      "name": "something",
      "value": [ false, true, true, false ]
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());
  std::vector<bool> expected = {false, true, true, false};
  EXPECT_EQ(attribute.GetBools(), expected);
}

// In this test we expect a crash because the "value" field in the attribute is
// invalid.
TEST(IppAttributeGetBools, InvalidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "boolean",
      "name": "something",
      "value": [ false, true, true, false, "invalid" ]
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());
  EXPECT_DEATH(attribute.GetBools(), "Failed to retrieve boolean");
}

TEST(IppAttributeGetInt, ValidAttributes) {
  const std::string json_contents1 = R"(
    { "type": "integer", "name": "copies-default", "value": 1 }
  )";
  std::optional<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1->GetDict());
  EXPECT_EQ(attribute1.GetInt(), 1);

  const std::string json_contents2 = R"(
    { "type": "integer", "name": "pages-per-minute", "value": 27 }
  )";
  std::optional<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2->GetDict());
  EXPECT_EQ(attribute2.GetInt(), 27);
}

// In this test we expect crashes because the "value" fields in the attributes
// are invalid.
TEST(IppAttributeGetInt, InvalidAttributes) {
  const std::string json_contents1 = R"(
    { "type": "integer", "name": "copies-default", "value": 1.33 }
  )";
  std::optional<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1->GetDict());
  EXPECT_DEATH(attribute1.GetInt(), "Failed to retrieve integer");

  const std::string json_contents2 = R"(
    { "type": "integer", "name": "pages-per-minute", "value": "invalid" }
  )";
  std::optional<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2->GetDict());
  EXPECT_DEATH(attribute2.GetInt(), "Failed to retrieve integer");
}

TEST(IppAttributeGetInts, ValidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "integer",
      "name": "media-bottom-margin-supported",
      "value": [ 511, 1023 ]
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());
  std::vector<int> expected = {511, 1023};
  EXPECT_EQ(attribute.GetInts(), expected);
}

// In this test we expect a crash because the "value" field in the attribute is
// invalid.
TEST(IppAttributeGetInts, InvalidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "integer",
      "name": "media-bottom-margin-supported",
      "value": [ 511, 1023, 1.33 ]
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());
  EXPECT_DEATH(attribute.GetInts(), "Failed to retrieve integer");
}

TEST(IppAttributeGetString, ValidAttributes) {
  const std::string json_contents1 = R"(
    { "type": "charset", "name": "attributes-charset", "value": "utf-8" }
  )";
  std::optional<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1->GetDict());
  EXPECT_EQ(attribute1.GetString(), "utf-8");

  const std::string json_contents2 = R"(
    { "type": "keyword", "name": "compression-supported", "value": "none" }
  )";
  std::optional<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2->GetDict());
  EXPECT_EQ(attribute2.GetString(), "none");
}

// In this test we expect crashes because the "value" fields in the attributes
// are invalid.
TEST(IppAttributeGetString, InvalidAttributes) {
  const std::string json_contents1 = R"(
    { "type": "charset", "name": "attributes-charset", "value": false }
  )";
  std::optional<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1->GetDict());
  EXPECT_DEATH(attribute1.GetString(), "Failed to retrieve string");

  const std::string json_contents2 = R"(
    { "type": "keyword", "name": "compression-supported", "value": 123 }
  )";
  std::optional<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2->GetDict());
  EXPECT_DEATH(attribute2.GetString(), "Failed to retrieve string");
}

TEST(IppAttributeGetStrings, ValidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "keyword",
      "name": "which-jobs-supported",
      "value": [ "completed", "not-completed" ]
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());
  std::vector<std::string> expected = {"completed", "not-completed"};
  EXPECT_EQ(attribute.GetStrings(), expected);
}

// In this test we expect a crash because the "value" field in the attribute is
// invalid.
TEST(IppAttributeGetStrings, InvalidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "keyword",
      "name": "which-jobs-supported",
      "value": [ "completed", false ]
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());
  EXPECT_DEATH(attribute.GetStrings(), "Failed to retrieve string");
}

TEST(IppAttributeGetBytes, ValidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "dateTime",
      "name": "printer-config-change-date-time",
      "value": [ 7, 255, 8, 20, 15, 49, 49, 0, 45, 8, 0 ]
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());
  std::vector<uint8_t> expected = {7, 255, 8, 20, 15, 49, 49, 0, 45, 8, 0};
  EXPECT_EQ(attribute.GetBytes(), expected);
}

TEST(IppAttributeGetBytes, InvalidAttributes) {
  const std::string json_contents1 = R"(
    {
      "type": "dateTime",
      "name": "printer-config-change-date-time",
      "value": [ 49, false, 18 ]
    }
  )";
  std::optional<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1->GetDict());
  EXPECT_DEATH(attribute1.GetBytes(), "Failed to retrieve byte value");

  const std::string json_contents2 = R"(
    {
      "type": "dateTime",
      "name": "printer-config-change-date-time",
      "value": [ 23, 1, -1 ]
    }
  )";
  std::optional<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2->GetDict());
  EXPECT_DEATH(attribute2.GetBytes(), "Retrieved byte value is negative");

  const std::string json_contents3 = R"(
    {
      "type": "dateTime",
      "name": "printer-config-change-date-time",
      "value": [ 7, 255, 20, 15, 256 ]
    }
  )";
  std::optional<base::Value> value3 = GetJSONValue(json_contents3);
  IppAttribute attribute3 = GetAttribute(value3->GetDict());
  EXPECT_DEATH(attribute3.GetBytes(), "Retrieved byte value is too large");
}

TEST(AddPrinterAttributes, ValidAttributes) {
  const std::string json_contents = R"(
  {
    "operationAttributes": [{
      "type": "charset",
      "name": "attributes-charset",
      "value": "utf-8"
    }, {
      "type": "naturalLanguage",
      "name": "attributes-natural-language",
      "value": "en-us"
    }]
  })";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  std::vector<IppAttribute> attributes =
      GetAttributes(value->GetDict(), kOperationAttributes);

  const std::string expected_string =
      "\x01"                         // Tag for operation-attributes.
      "\x47"                         // Type specifier for charset.
      "\x00\x12"                     // Length of name.
      "attributes-charset"           // Name.
      "\x00\x05"                     // Length of value.
      "utf-8"                        // Value.
      "\x48"                         // Type specifier for naturalLanguage.
      "\x00\x1b"                     // Length of name.
      "attributes-natural-language"  // Name.
      "\x00\x05"                     // Length of value.
      "en-us"s;                      // Value.
  const std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddPrinterAttributes(attributes, kOperationAttributes, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddBoolean, SingleValue) {
  const std::string json_contents = R"(
    { "type": "boolean", "name": "color-supported", "value": false }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());

  const std::string expected_string =
      "\x22"             // Type specifier for boolean.
      "\x00\x0f"         // Length of name.
      "color-supported"  // Name.
      "\x00\x01"         // Length of value.
      "\x00"s;           // Value.
  const std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddBoolean(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddBoolean, MultipleValues) {
  const std::string json_contents = R"(
    {
      "type": "boolean",
      "name": "color-supported",
      "value": [ false, true, false, false ]
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());

  const std::string expected_string =
      "\x22"                  // Type specifier for boolean.
      "\x00\x0f"              // Length of name.
      "color-supported"       // Name.
      "\x00\x01"              // Length of value.
      "\x00"                  // Value.
      "\x22\x00\x00\x00\x01"  // Repeated value.
      "\x01"                  // Value.
      "\x22\x00\x00\x00\x01"  // Repeated value.
      "\x00"                  // Value.
      "\x22\x00\x00\x00\x01"  // Repeated value.
      "\x00"s;                // Value.
  std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddBoolean(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddInteger, SingleValue) {
  const std::string json_contents = R"(
    { "type": "integer", "name": "copies-default", "value": 1 }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());

  const std::string expected_string =
      "\x21"                // Type specifier for integer.
      "\x00\x0e"            // Length of name.
      "copies-default"      // Name.
      "\x00\x04"            // Length of value.
      "\x00\x00\x00\x01"s;  // Value.
  std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddInteger(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddInteger, MultipleValues) {
  const std::string json_contents = R"(
    {
      "type": "integer",
      "name": "media-top-margin-supported",
      "value": [ 511, 1023 ]
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());

  const std::string expected_string =
      "\x21"                        // Type specifier for integer.
      "\x00\x1a"                    // Length of name.
      "media-top-margin-supported"  // Name.
      "\x00\x04"                    // Length of value.
      "\x00\x00\x01\xff"            // Value.
      "\x21\x00\x00\x00\x04"        // Repeated value.
      "\x00\x00\x03\xff"s;          // Value.
  std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddInteger(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddString, SingleValue) {
  const std::string json_contents = R"(
    { "type": "keyword", "name": "compression-supported", "value": "none" }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());

  const std::string expected_string =
      "\x44"                   // Type specifier for keyword.
      "\x00\x15"               // Length of name.
      "compression-supported"  // Name.
      "\x00\x04"               // Length of value.
      "none"s;                 // Value.
  std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddString(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddString, MultipleValues) {
  const std::string json_contents = R"(
    {
      "type": "keyword",
      "name": "print-color-mode-supported",
      "value": [ "auto", "auto-monochrome", "monochrome" ]
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());

  const std::string expected_string =
      "\x44"                        // Type specifier for keyword.
      "\x00\x1a"                    // Length of name.
      "print-color-mode-supported"  // Name.
      "\x00\x04"                    // Length of value.
      "auto"                        // Value.
      "\x44\x00\x00\x00\x0f"        // Repeated value.
      "auto-monochrome"             // Value.
      "\x44\x00\x00\x00\x0a"        // Repeated value.
      "monochrome"s;                // Value.
  std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddString(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddDate, ValidDate) {
  const std::string json_contents = R"(
    {
      "type": "dateTime",
      "name": "printer-config-change-date-time",
      "value": [ 7, 255, 8, 20, 15, 49, 49, 0, 45, 8, 0 ]
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());

  const std::string expected_string =
      "\x31"                             // Type specifier for dateTime.
      "\x00\x1f"                         // Length of name.
      "printer-config-change-date-time"  // Name.
      "\x00\x0b"                         // Length of value.
      "\x07\xff\x08\x14\x0f\x31\x31\x00\x2d\x08\x00"s;  // Value.
  std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddDate(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

// We expect this test to fail because the "value" field for the "dateTime"
// attribute is not a list.
TEST(AddDate, InvalidDate) {
  const std::string json_contents = R"(
    {
      "type": "dateTime",
      "name": "printer-config-change-date-time",
      "value": "not a list"
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());
  SmartBuffer result(0);
  EXPECT_DEATH(AddDate(attribute, &result),
               "Date value is in an incorrect format");
}

TEST(AddOctetString, StringValue) {
  const std::string json_contents = R"(
    {
      "type": "octetString",
      "name": "printer-input-tray",
      "value": "type=other;mediafeed=0;mediaxfeed=0;maxcapacity=250;level=-2"
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());

  const std::string expected_string =
      "\x30"                // Type specifier for octetString.
      "\x00\x12"            // Length of name.
      "printer-input-tray"  // Name.
      "\x00\x3c"            // Length of value.
      // Value
      "type=other;mediafeed=0;mediaxfeed=0;maxcapacity=250;level=-2"s;
  const std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddOctetString(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddOctetString, BytesValue) {
  const std::string json_contents = R"(
    {
      "type": "octetString",
      "name": "printer-firmware-version",
      "value": [0, 4, 15, 6]
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());

  const std::string expected_string =
      "\x30"                      // Type specifier for octetString.
      "\x00\x18"                  // Length of name.
      "printer-firmware-version"  // Name.
      "\x00\x04"                  // Length of array.
      "\x00\x04\x0f\x06"s;        // Array elements.
  const std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddOctetString(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddRange, ValidRange) {
  const std::string json_contents = R"(
    {
      "type": "rangeOfInteger",
      "name": "copies-supported",
      "value": [ 1, 1023 ]
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());

  const std::string expected_string =
      "\x33"                // Type specifier for range.
      "\x00\x10"            // Length of name.
      "copies-supported"    // Name.
      "\x00\x08"            // Length of range.
      "\x00\x00\x00\x01"    // Range lower bound.
      "\x00\x00\x03\xff"s;  // Range upper bound.
  std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddRange(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

// We expect this case to fail because the "value" field for the
// "rangeOfInteger" field is not a list.
TEST(AddRange, InvalidRange) {
  const std::string json_contents = R"(
    {
      "type": "rangeOfInteger",
      "name": "copies-supported",
      "value": "1-1023"
    }
  )";
  SmartBuffer result(0);
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());
  EXPECT_DEATH(AddRange(attribute, &result),
               "Range value is in an incorrect format");
}

TEST(AddResolution, ValidResolution) {
  const std::string json_contents = R"(
    {
      "type": "resolution",
      "name": "printer-resolution-default",
      "value": [ 300, 300, 3 ]
    }
  )";
  std::optional<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value->GetDict());

  const std::string expected_string =
      "\x32"                        // Type specifier for resolution.
      "\x00\x1a"                    // Length of name.
      "printer-resolution-default"  // Name.
      "\x00\x09"                    // Length of resolution value.
      "\x00\x00\x01\x2c"            // Cross-feed (X) resolution size.
      "\x00\x00\x01\x2c"            // Feed (Y) resolution size.
      "\x03"s;                      // Unit type.
  const std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddResolution(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddResolution, InvalidResolution) {
  // We expect this case to fail because the "value" field for the "resolution"
  // attribute is not a list.
  const std::string json_contents1 = R"(
    {
      "type": "resolution",
      "name": "copies-supported",
      "value": "300, 300, 3"
    }
  )";
  std::optional<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1->GetDict());
  SmartBuffer result(0);
  EXPECT_DEATH(AddResolution(attribute1, &result),
               "Resolution value is in an incorrect format");

  // We expect this case to fail because the "value" field for the "resolution"
  // attribute is not a list.
  const std::string json_contents2 = R"(
    {
      "type": "resolution",
      "name": "copies-supported",
      "value": [ 300, 300, 3, 4 ]
    }
  )";
  std::optional<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2->GetDict());
  EXPECT_DEATH(AddResolution(attribute2, &result),
               "Resolution list is an invalid size");
}

}  // namespace
