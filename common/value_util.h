// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMMON_VALUE_UTIL_H_
#define COMMON_VALUE_UTIL_H_

#include <memory>
#include <optional>
#include <string>

#include <base/json/json_reader.h>
#include <base/values.h>

// Attempt to load the contents of the JSON file located at `file_path` and
// return the contents in a string.
std::optional<std::string> GetJSONContents(const std::string& file_path);

// Use a JSONReader to parse `json_contents` and return a pointer to the
// underlying Value object.
std::optional<base::Value> GetJSONValue(const std::string& json_contents);

#endif  // COMMON_VALUE_UTIL_H_
