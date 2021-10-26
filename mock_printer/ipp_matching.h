// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_IPP_MATCHING_H_
#define MOCK_PRINTER_IPP_MATCHING_H_

// This header declares functions used to match IPP Protobufs against
// IPP messages parsed by libipp.

#include <chromeos/libipp/ipp.h>

#include "mock_printer/proto/ipp.pb.h"

namespace mock_printer {

// Returns whether or not `message` matches `mock_message`.
bool IppMessageMatches(const ipp::Request* message,
                       const mocking::IppMessage& mock_message);

}  // namespace mock_printer

#endif  // MOCK_PRINTER_IPP_MATCHING_H_
