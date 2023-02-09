// Copyright 2022 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "proto_to_http.h"

#include <optional>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_printer/proto/http.pb.h"
#include "parse_textproto.h"

namespace mock_printer {
namespace {

// POD struct for ToHttpStatusConversionTest's StatusConversion() test.
struct ToHttpStatusConversionParams {
  // Textproto to parse.
  std::string textproto;
  // Expected HTTP status code.
  std::string http_status_code;
};

TEST(ToHttpStatus, StatusConversionNoHttpProperties) {
  EXPECT_EQ(ToHttpStatus(std::nullopt), "200 OK");
}

class ToHttpStatusConversionTest
    : public testing::Test,
      public testing::WithParamInterface<ToHttpStatusConversionParams> {
 protected:
  ToHttpStatusConversionTest() = default;
  ToHttpStatusConversionTest(const ToHttpStatusConversionTest&) = delete;
  ToHttpStatusConversionTest& operator=(const ToHttpStatusConversionTest&) =
      delete;

  ToHttpStatusConversionParams params() const { return GetParam(); }
};

TEST_P(ToHttpStatusConversionTest, StatusConversion) {
  mocking::HttpProperties http_properties =
      ParseHttpPropertiesOrDie(params().textproto);
  EXPECT_EQ(ToHttpStatus(http_properties), params().http_status_code);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ToHttpStatusConversionTest,
                         testing::Values(
                             ToHttpStatusConversionParams{
                                 R"pb(status_code: UNSPECIFIED)pb", "200 OK"},
                             ToHttpStatusConversionParams{
                                 R"pb(status_code: INTERNAL_SERVER_ERROR)pb",
                                 "500 Internal Server Error"}));

}  // namespace
}  // namespace mock_printer
