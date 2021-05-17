// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "argument_variants.h"

namespace mock_printer {

MatchableIppData::MatchableIppData(const ipp::Request* message,
                                   const mocking::IppMessage& mock_message)
    : ipp_request(message), mock_ipp_request(mock_message) {}
MatchableIppData::~MatchableIppData() = default;

}  // namespace mock_printer
