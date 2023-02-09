// Copyright 2022 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_PRINTER_PROTO_TO_HTTP_H_
#define MOCK_PRINTER_PROTO_TO_HTTP_H_

// This module is defined to convert HttpProperties from the mocking protobufs
// to a format recognized by HttpResponse.

#include <optional>
#include <string>

#include "mock_printer/proto/http.pb.h"

namespace mock_printer {

// Get an appropriate HTTP status code from the mocking protobuf's
// HttpProperties.
std::string ToHttpStatus(
    const std::optional<mocking::HttpProperties>& http_properties);

}  // namespace mock_printer

#endif  // MOCK_PRINTER_PROTO_TO_HTTP_H_
