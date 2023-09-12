// Copyright 2023 The ChromiumOS Authors
// // Use of this source code is governed by a BSD-style license that can be
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
    // wait for host to ready.
    sleep(3);  // FIXME(santo): Confirm the sleep time is enough.
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

  base::test::TaskEnvironment task_environment_;
  struct sockaddr_in serv_addr;
  int client_fd = -1;
  static base::Process proc;
};

base::Process UsbIpManagerTest::proc;

TEST_F(UsbIpManagerTest, StartUsbIpHost) {
  ASSERT_EQ(proc.IsValid(), true);
}

TEST_F(UsbIpManagerTest, CheckConnection) {
  EXPECT_GT(client_fd, 0);
}

TEST_F(UsbIpManagerTest, HandleDeviceList) {
  EXPECT_GT(client_fd, 0);
  OpHeader header;
  header.version = 2;
  header.command = OP_REQ_DEVLIST_CMD;
  header.status = 0;
  SmartBuffer buf = PackOpHeader(header);
  SendBuffer(client_fd, buf);

  OpRepDevlist list;
  recv(client_fd, &list, sizeof(list), 0);
  int count = ntohs(list.header.numExportedDevices);
  close(client_fd);
  EXPECT_EQ(count, 0);
}
