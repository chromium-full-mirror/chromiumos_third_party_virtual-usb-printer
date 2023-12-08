// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_LIBIPP_HELPER_H_
#define MOCK_PRINTER_LIBIPP_HELPER_H_

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <chromeos/libipp/frame.h>

namespace mock_printer {

// Defines a struct that has a stream operator allowing it to emit
// multiline logs.
struct MultilineLogHolder {
  std::vector<std::string> lines;

  // Emits the `preface` and all `lines` at the ERROR level.
  //
  // Does nothing if `lines` is empty.
  //
  // You can't parameterize the level here because there's some
  // preprocessor magic involved in base/logging.h.
  void LogError(std::string_view preface = "") const;

  // `VLOG()`s the `preface` and all `lines` at the specified `level`.
  //
  // Does nothing if `lines` is empty.
  void Vlog(int level, std::string_view preface = "") const;
};

// Returns a vector of lines that form a human-readable string
// representation of `request`.
//
// Note that a more sophisticated version of this function (albeit for
// `ipp::Response`) exists in `platform2/print_tools`.
MultilineLogHolder ToString(const ipp::Frame& request);

}  // namespace mock_printer

#endif  // MOCK_PRINTER_LIBIPP_HELPER_H_
