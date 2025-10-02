// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "value_util.h"

#include <optional>
#include <utility>

#include <base/check.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/values.h>

std::optional<std::string> GetJSONContents(const std::string& file_path) {
  std::string json_contents;
  const base::FilePath path(file_path);
  if (!base::ReadFileToString(path, &json_contents)) {
    return {};
  }
  return json_contents;
}

std::optional<base::Value> GetJSONValue(const std::string& json_contents) {
  std::optional<base::Value> value = base::JSONReader::Read(
      json_contents, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  CHECK(value) << "Failed to parse JSON string";
  return value;
}
