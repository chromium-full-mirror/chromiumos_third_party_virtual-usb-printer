// Copyright 2022 The ChromiumOS Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_printer.h"
#include "http_util.h"

#include <optional>
#include <string>
#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/files/file_enumerator.h>
#include <base/strings/stringprintf.h>
#include <gtest/gtest.h>

namespace {

std::unique_ptr<UsbPrinter> CreateUsbPrinter(const base::FilePath& httpLogDir) {
  const std::string descriptorsPath("./config/ippusb_printer.json");
  std::optional<UsbDescriptors> usbDescriptors =
      UsbDescriptors::Create(descriptorsPath);
  if (!usbDescriptors.has_value()) {
    return std::unique_ptr<UsbPrinter>();
  }

  const base::FilePath documentOutputPath;
  std::unique_ptr<mock_printer::MockPrinter> mockPrinter;
  ScannerCapabilities caps;
  EsclManager esclManager(caps, base::FilePath());
  IppManager ippManager;

  return std::make_unique<UsbPrinter>(
      usbDescriptors.value(), documentOutputPath, httpLogDir, ippManager,
      esclManager, std::move(mockPrinter));
}

}  // namespace

TEST(UsbPrinter, LogHttpHeaders) {
  // Temp directory to store our HTTP header log files.
  base::ScopedTempDir httpLogDir;
  ASSERT_TRUE(httpLogDir.CreateUniqueTempDir());

  std::unique_ptr<UsbPrinter> printer = CreateUsbPrinter(httpLogDir.GetPath());
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
