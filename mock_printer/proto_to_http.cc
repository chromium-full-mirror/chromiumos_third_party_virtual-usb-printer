// Copyright 2022 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "proto_to_http.h"

#include <string>

#include <base/notreached.h>

#include "mock_printer/proto/http.pb.h"

namespace mock_printer {

namespace {

std::string StatusCodeFromProto(
    const mocking::HttpProperties::StatusCode proto_status) {
  switch (proto_status) {
    case mocking::HttpProperties::UNSPECIFIED:
      // mocking::HttpProperties' status_code field is optional. If it isn't
      // set, we assume some other property of the HTTP response is mocked and
      // set a default status code.
      return "200 OK";
    case mocking::HttpProperties::INTERNAL_SERVER_ERROR:
      return "500 Internal Server Error";
    default:
      NOTREACHED()
          << "BUG: unhandled `mocking::HttpProperties::StatusCode` value case";
  }
}

}  // namespace

std::string ToHttpStatus(
    const std::optional<mocking::HttpProperties>& http_properties) {
  if (!http_properties.has_value()) {
    return "200 OK";
  }

  return StatusCodeFromProto(http_properties.value().status_code());
}

}  // namespace mock_printer
