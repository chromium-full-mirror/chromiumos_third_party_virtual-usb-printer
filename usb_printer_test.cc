// Copyright 2022 The ChromiumOS Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_printer.h"
#include "http_util.h"
#include "value_util.h"
#include "mock_printer/parse_textproto.h"

#include <optional>
#include <string>
#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/files/file_enumerator.h>
#include <base/strings/stringprintf.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

class UsbPrinterTestPeer {
 public:
  HttpResponse GenerateHttpResponse(UsbPrinter& printer,
                                    const HttpRequest& request,
                                    SmartBuffer* body) {
    return printer.GenerateHttpResponse(request, body);
  }
};

namespace {

std::unique_ptr<UsbPrinter> CreateUsbPrinter(
    const base::FilePath& httpLogDir,
    std::unique_ptr<mock_printer::MockPrinter> mockPrinter) {
  const std::string descriptorsPath("./config/ippusb_printer.json");
  std::optional<UsbDescriptors> usbDescriptors =
      UsbDescriptors::Create(descriptorsPath);
  if (!usbDescriptors.has_value()) {
    return std::unique_ptr<UsbPrinter>();
  }

  const base::FilePath documentOutputPath;
  ScannerCapabilities caps;
  EsclManager esclManager(caps, base::FilePath());
  IppManager ippManager;

  return std::make_unique<UsbPrinter>(
      usbDescriptors.value(), documentOutputPath, httpLogDir, ippManager,
      esclManager, std::move(mockPrinter));
}

std::unique_ptr<UsbPrinter> CreateUsbPrinter() {
  std::unique_ptr<mock_printer::MockPrinter> mockPrinter;
  return CreateUsbPrinter(base::FilePath(), std::move(mockPrinter));
}

std::unique_ptr<UsbPrinter> CreateUsbPrinterWithMockPrinter(
    std::unique_ptr<mock_printer::MockPrinter> mockPrinter) {
  return CreateUsbPrinter(base::FilePath(), std::move(mockPrinter));
}

std::unique_ptr<UsbPrinter> CreateUsbPrinterWithLogDir(
    const base::FilePath& httpLogDir) {
  std::unique_ptr<mock_printer::MockPrinter> mockPrinter;
  return CreateUsbPrinter(httpLogDir, std::move(mockPrinter));
}

}  // namespace

TEST(UsbPrinter, LogHttpHeaders) {
  // Temp directory to store our HTTP header log files.
  base::ScopedTempDir httpLogDir;
  ASSERT_TRUE(httpLogDir.CreateUniqueTempDir());

  std::unique_ptr<UsbPrinter> printer =
      CreateUsbPrinterWithLogDir(httpLogDir.GetPath());
  ASSERT_TRUE(printer);

  const std::string key1("key1");
  const std::string value1("value1");
  const std::string key2("key2");
  const std::string value2("value2 with spaces");
  const std::string header1(
      base::StringPrintf("%s: %s", key1.c_str(), value1.c_str()));
  const std::string header2(
      base::StringPrintf("%s: %s", key2.c_str(), value2.c_str()));
  HttpRequest request;
  request.request_line = "POST /ipp/print HTTP/1.1";
  request.headers[header1] = value1;
  printer->LogHttpHeaders(request);

  request.headers.clear();
  request.headers[header2] = value2;
  printer->LogHttpHeaders(request);

  // Loop over our files (there should be one file per LogHttpHeaders request).
  // Make sure the files contain the expected data.
  const std::string filename1("http-header-0001.txt");
  const std::string filename2("http-header-0002.txt");
  int numFiles = 0;
  base::FileEnumerator enumerator(httpLogDir.GetPath(), false,
                                  base::FileEnumerator::FILES,
                                  "http-header-*.txt");
  for (auto path = enumerator.Next(); !path.empty(); path = enumerator.Next()) {
    const std::string thisFilename = path.BaseName().value();
    if (thisFilename == filename1) {
      ++numFiles;
      std::string data;
      EXPECT_TRUE(base::ReadFileToString(path, &data));

      EXPECT_TRUE(data.find(request.request_line) != std::string::npos);
      EXPECT_TRUE(data.find(header1) != std::string::npos);
    } else if (thisFilename == filename2) {
      ++numFiles;
      std::string data;
      EXPECT_TRUE(base::ReadFileToString(path, &data));

      EXPECT_TRUE(data.find(request.request_line) != std::string::npos);
      EXPECT_TRUE(data.find(header2) != std::string::npos);
    } else {
      // We only expect two HTTP header files.  Fail if we see any others.
      FAIL() << "Unexpected file: " << path.value();
    }
  }

  EXPECT_EQ(numFiles, 2);
}

// Define bytes for Get-Attributes request and response.
static const std::vector<uint8_t> kGetAttributesRequest{2, 0, 0, 11, 0,
                                                        0, 0, 1, 3};
static const std::vector<uint8_t> kExpectedGetAttributesResponse{
    2, 0, 0, 0, 0, 0, 0, 1, 1, 4, 3};

TEST(UsbPrinter, ResponseWithNoMockPrinter) {
  // We are not going to use a mock_printer for this test case, so we expect the
  // results to be what our usb_printer would normally respond with, which is
  // determined by the config used to create the usb printer.

  std::unique_ptr<UsbPrinter> printer = CreateUsbPrinter();
  ASSERT_TRUE(printer);

  UsbPrinterTestPeer printerTestPeer;
  HttpRequest request{"", "POST", "/ipp/print"};
  SmartBuffer data(kGetAttributesRequest);
  HttpResponse response =
      printerTestPeer.GenerateHttpResponse(*printer.get(), request, &data);

  EXPECT_EQ(response.status, "200 OK");
  EXPECT_THAT(response.body.contents(), kExpectedGetAttributesResponse);
}

TEST(UsbPrinter, MockPrinterDefaultResponse) {
  // For this test, we use a mock_printer but we script the response to use the
  // default response, so we expect the same results that the usb printer would
  // provide with no mock printer.

  const auto test_case = mock_printer::ParseTestCaseOrDie(R"pb(
    steps {
      cardinality { type: ALL_IN_ORDER }
      expectation_with_response {
        expectation {
          ipp_matcher {
            operation_id: 0x000b
            groups { tag: OPERATION_ATTRIBUTES }
          }
        }
        response { default_response: true }
      }
    }
  )pb");

  std::unique_ptr<mock_printer::MockPrinter> mock_printer =
      mock_printer::MockPrinter::Create(test_case);

  std::unique_ptr<UsbPrinter> printer =
      CreateUsbPrinterWithMockPrinter(std::move(mock_printer));
  ASSERT_TRUE(printer);

  UsbPrinterTestPeer printerTestPeer;
  HttpRequest request{"", "POST", "/ipp/print"};
  SmartBuffer data(kGetAttributesRequest);
  HttpResponse response =
      printerTestPeer.GenerateHttpResponse(*printer.get(), request, &data);

  EXPECT_EQ(response.status, "200 OK");
  EXPECT_THAT(response.body.contents(), kExpectedGetAttributesResponse);
}

TEST(UsbPrinter, MockPrinterScriptedResponse) {
  // For this test, we use a mock_printer with a scripted response.  We will
  // check the response from the usb printer to make sure it matches the
  // scripted response.

  const auto test_case = mock_printer::ParseTestCaseOrDie(R"pb(
    steps {
      cardinality { type: ALL_IN_ORDER }
      expectation_with_response {
        expectation {
          ipp_matcher {
            operation_id: 0x000b
            groups { tag: OPERATION_ATTRIBUTES }
          }
        }
        response { http_properties { status_code: 500 } }
      }
    }
  )pb");

  std::unique_ptr<mock_printer::MockPrinter> mock_printer =
      mock_printer::MockPrinter::Create(test_case);

  std::unique_ptr<UsbPrinter> printer =
      CreateUsbPrinterWithMockPrinter(std::move(mock_printer));
  ASSERT_TRUE(printer);

  UsbPrinterTestPeer printerTestPeer;
  HttpRequest request{"", "POST", "/ipp/print"};
  SmartBuffer data(kGetAttributesRequest);
  HttpResponse response =
      printerTestPeer.GenerateHttpResponse(*printer.get(), request, &data);

  EXPECT_EQ(response.status, "500 Internal Server Error");
  EXPECT_EQ(response.body.size(), 0);
}
