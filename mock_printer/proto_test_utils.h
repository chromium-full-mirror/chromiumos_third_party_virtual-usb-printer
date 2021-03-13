// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_PROTO_TEST_UTILS_H_
#define MOCK_PRINTER_PROTO_TEST_UTILS_H_

#include <string>

#include "proto/ipp.pb.h"

// Declares functions for manipulating the Mock Printer Protobufs
// (defined in proto/) in unit tests.

namespace mock_printer {

// Returns a `mocking::IppMessage` parsed from `textproto`.
mocking::IppMessage ParseTestProto(const std::string& textproto);

}  // namespace mock_printer

#endif  // MOCK_PRINTER_PROTO_TEST_UTILS_H_
