// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_LIBIPP_HELPER_H_
#define MOCK_PRINTER_LIBIPP_HELPER_H_

#include <string>
#include <vector>

#include <chromeos/libipp/ipp.h>

namespace mock_printer {

// Returns a vector of lines that form a human-readable string
// representation of `request`.
//
// Note that a more sophisticated version of this function (albeit for
// `ipp::Response`) exists in `platform2/print_tools`.
std::vector<std::string> ToString(const ipp::Request* request);

}  // namespace mock_printer

#endif  // MOCK_PRINTER_LIBIPP_HELPER_H_
