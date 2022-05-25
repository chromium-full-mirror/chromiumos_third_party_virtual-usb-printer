// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_PROTO_TO_LIBIPP_H_
#define MOCK_PRINTER_PROTO_TO_LIBIPP_H_

// This module is defined to create `libipp` response messages out of
// the mocking Protobufs.

#include <memory>

#include <chromeos/libipp/frame.h>

#include "mock_printer/proto/ipp.pb.h"

namespace mock_printer {

std::unique_ptr<ipp::Frame> ToResponse(
    const mocking::IppMessage& mock_response);

}  // namespace mock_printer

#endif  // MOCK_PRINTER_PROTO_TO_LIBIPP_H_
