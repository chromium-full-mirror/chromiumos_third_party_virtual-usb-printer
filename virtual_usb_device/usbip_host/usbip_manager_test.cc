// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip_manager.h"

#include <base/strings/stringprintf.h>
#include <base/command_line.h>
#include <base/process/launch.h>
#include <base/process/process.h>
#include <base/path_service.h>
#include <base/base_paths.h>
#include <base/files/file_util.h>
#include <base/test/task_environment.h>
#include <gtest/gtest.h>

#include "virtual-usb-printer/virtual_usb_device/http_util.h"
#include "virtual-usb-printer/virtual_usb_device/value_util.h"
#include "virtual-usb-printer/virtual_usb_device/op_commands.h"

class UsbIpManagerTest : public testing::Test {
 public:
  static void SetUpTestSuite() {
    const char kProcSelfExe[] = "/proc/self/exe";
    base::FilePath self_bin;
    if (!base::ReadSymbolicLink(base::FilePath(kProcSelfExe), &self_bin)) {
      NOTREACHED() << "Unable to resolve " << kProcSelfExe << ".";
    }
    base::FilePath host_bin = self_bin.DirName().Append("virtual-usbip-host");
    base::CommandLine cmd(host_bin);
    proc = base::LaunchProcess(cmd, base::LaunchOptionsForTest());
    sleep(3);  // wait for host to ready
  }

  static void TearDownTestSuite() { proc.Terminate(0, true); }

 protected:
  void SetUp() override {
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(3240);

    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    int status =
        connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (status < 0) {
      LOG(ERROR) << "Connection failed";
    }
  }

  void TearDown() override {
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
  }

  base::test::TaskEnvironment task_environment_;
  struct sockaddr_in serv_addr;
  int client_fd = -1;
  static base::Process proc;
  base::Process printer_proc;
};

base::Process UsbIpManagerTest::proc;

TEST_F(UsbIpManagerTest, StartUsbIpHost) {
  ASSERT_EQ(proc.IsValid(), true);
}

TEST_F(UsbIpManagerTest, CheckConnection) {
  EXPECT_GT(client_fd, 0);
}

TEST_F(UsbIpManagerTest, HandleDeviceListNoDevice) {
  EXPECT_GT(client_fd, 0);
  OpHeader header;
  header.version = 2;
  header.command = OP_REQ_DEVLIST_CMD;
  header.status = 0;
  SmartBuffer buf1 = PackOpHeader(header);
  SendBuffer(client_fd, buf1);

  SmartBuffer buf = ReceiveBuffer(client_fd, sizeof(OpRepDevlistHeader));
  OpRepDevlistHeader list_header = UnpackOpRepDevlistHeader(&buf);
  int count = list_header.numExportedDevices;
  EXPECT_EQ(count, 0);
}

TEST_F(UsbIpManagerTest, HandleDeviceListOneDevice) {
  EXPECT_GT(client_fd, 0);
  BindRequest request;
  request.header.version = 2;
  request.header.command = OP_REQ_BIND_CMD;
  request.port = 3245;
  strncpy(request.device_desc.device.usbPath, "/sys/path/", 10);
  strncpy(request.device_desc.device.usbPath, "1:2", 3);
  request.device_desc.device.busnum = 1;
  request.device_desc.device.devnum = 2;
  request.device_desc.device.bNumInterfaces = 0;
  SmartBuffer buf = PackBindRequest(request);
  SendBuffer(client_fd, buf);
  close(client_fd);

  client_fd = socket(AF_INET, SOCK_STREAM, 0);
  int status =
      connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
  if (status < 0) {
    LOG(ERROR) << "Connection failed";
  }

  OpHeader header;
  header.version = 2;
  header.command = OP_REQ_DEVLIST_CMD;
  header.status = 0;
  SmartBuffer buf1 = PackOpHeader(header);
  SendBuffer(client_fd, buf1);

  SmartBuffer buf2 = ReceiveBuffer(client_fd, sizeof(OpRepDevlistHeader));
  OpRepDevlistHeader list_header = UnpackOpRepDevlistHeader(&buf2);
  int count = list_header.numExportedDevices;

  EXPECT_EQ(count, 1);
}
