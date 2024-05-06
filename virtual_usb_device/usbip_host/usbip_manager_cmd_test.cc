// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip_manager.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/task/thread_pool/thread_pool_instance.h>
#include <base/threading/platform_thread.h>
#include <base/threading/simple_thread.h>
#include <base/time/time.h>
#include <gtest/gtest.h>

#include "device_proxy.h"
#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/virtual_usb_device/connection/socket_connection.h"
#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/op_commands.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/usb_device.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/virtual_usb_device/usbip_constants.h"

// A `DeviceProxy` class implementation that communicates with
// usb device using direct function call.
class StubDeviceProxy : public DeviceProxy {
 public:
  static std::unique_ptr<DeviceProxy> Create(const uint16_t port,
                                             const ::BusId& id,
                                             const size_t devnum) {
    return std::make_unique<StubDeviceProxy>(port, id, devnum);
  }

  StubDeviceProxy(const uint16_t port, const ::BusId& id, const size_t devnum)
      : DeviceProxy(port, id, devnum) {
    const std::string descriptorsPath(
        "/usr/local/etc/virtual-usb-printer/usb_printer.json");
    std::optional<UsbDescriptors> usbDescriptors =
        UsbDescriptors::Create(descriptorsPath);
    if (usbDescriptors.has_value()) {
      usb_device_.reset(new UsbDevice(usbDescriptors.value()));
    }
  }
  ~StubDeviceProxy() override = default;
  bool ConnectDevice() override { return true; }

  // Makes a direct function call to `UsbDevice` to get the response
  // data.
  bool HandleUsbDeviceRequest(const Urb& urb_request,
                              const SmartBuffer& data,
                              SmartBuffer* response_data,
                              bool* stalled) override {
    if (!usb_device_)
      return false;

    *stalled = false;
    std::optional<SmartBuffer> result =
        usb_device_->HandleUsbRequest(urb_request, data);
    if (result.has_value())
      response_data->Add(result.value());
    else
      *stalled = true;

    return true;
  }

 private:
  std::unique_ptr<UsbDevice> usb_device_;
};

class TestThread : public base::SimpleThread {
 public:
  explicit TestThread(const uint16_t server_port)
      : SimpleThread("HostRunnerThread"), server_port_(server_port) {}

  void Run() override {
    DeviceProxy::CreateCb callback =
        base::BindRepeating(&StubDeviceProxy::Create);
    host_.SetCreateDeviceProxyCallback(callback);
    host_.Run(server_port_);
  }

  // This function is assumed to be threadsafe since
  // at the end it only calls `shutdown` for `socket`
  // which is assumed to be threadsafe.
  // The test will fail if `host_.Stop()` doesn't remain
  // threadsafe in future.
  void Stop() { host_.Stop(); }

 private:
  const uint16_t server_port_;
  UsbIpManager host_;
};

constexpr uint16_t SERVER_PORT = 3240;

class UsbIpManagerTest : public testing::Test {
 public:
  UsbIpManagerTest() : server_port_(SERVER_PORT) {}

 protected:
  static void SetUpTestSuite() {
    base::ThreadPoolInstance::Create("UsbIpHost Threadpool");
    constexpr size_t max_num_foreground_threads = 8;  // Chosen randomly.
    base::ThreadPoolInstance::InitParams init_params(
        max_num_foreground_threads);
    base::ThreadPoolInstance::Get()->Start(init_params);

    host_runner_thread_ = new TestThread(SERVER_PORT);
    host_runner_thread_->Start();
  }

  static void TearDownTestSuite() {
    host_runner_thread_->Stop();
    host_runner_thread_->Join();
    base::ThreadPoolInstance::Get()->Shutdown();
    delete host_runner_thread_;
  }

  void SetUp() override { WaitUntilServerReady(); }

  void WaitUntilServerReady() {
    size_t timeout = 300;  // Randomly chosen
    auto conn = std::make_unique<ClientSocketConnection>(server_port_);
    while (!conn->Start() && timeout > 0) {
      base::PlatformThread::Sleep(base::Seconds(1));
      timeout--;
    }
    if (timeout == 0)
      LOG(ERROR) << "Couldn't connect to server, tried 99 times";
    else
      LOG(INFO) << "Server is ready for connection";
  }

 protected:
  static TestThread* host_runner_thread_;
  const uint16_t server_port_;
};

TestThread* UsbIpManagerTest::host_runner_thread_ = nullptr;

TEST_F(UsbIpManagerTest, HandleDeviceListNoDevice) {
  auto conn = std::make_unique<ClientSocketConnection>(server_port_);
  ASSERT_TRUE(conn->Start());
  OpHeader header;
  header.version = 2;
  header.command = OP_REQ_DEVLIST_CMD;
  header.status = 0;
  SmartBuffer request_buf = PackOpHeader(header);
  conn->Send(request_buf);

  SmartBuffer response_buf = conn->Receive(sizeof(OpRepDevlistHeader));
  OpRepDevlistHeader list_header = UnpackOpRepDevlistHeader(&response_buf);
  int count = list_header.numExportedDevices;
  EXPECT_EQ(count, 0);
}

TEST_F(UsbIpManagerTest, HandleDeviceListOneDevice) {
  // Verify device registration.
  {
    auto conn = std::make_unique<ClientSocketConnection>(server_port_);
    ASSERT_TRUE(conn->Start());
    EXPECT_GT(conn->FD(), 0);

    // Test OP_REQ_BIND_CMD request.
    BindOrUnbindRequest request;
    request.header.version = 2;
    request.header.command = OP_REQ_BIND_CMD;
    request.port = 3245;  // device port
    SmartBuffer request_buf = PackBindOrUnbindRequest(request);
    LOG(INFO) << "UsbIpManagerTest: send bind command";
    conn->Send(request_buf);

    SmartBuffer response_buf = conn->Receive(sizeof(OpHeader));
    OpHeader header = UnpackOpHeader(&response_buf);
    LOG(INFO) << "UsbIpManagerTest: received bind response "
              << (header.status ? "failure" : "success");
    EXPECT_EQ(header.status, 0);
  }

  // Verify device listing.
  {
    auto conn = std::make_unique<ClientSocketConnection>(server_port_);
    ASSERT_TRUE(conn->Start());
    OpHeader header;
    header.version = 2;
    header.command = OP_REQ_DEVLIST_CMD;
    header.status = 0;
    SmartBuffer request_buf = PackOpHeader(header);
    LOG(INFO) << "UsbIpManagerTest: send device list command";
    conn->Send(request_buf);

    SmartBuffer response_buf = conn->Receive(sizeof(OpRepDevlistHeader));
    OpRepDevlistHeader list_header = UnpackOpRepDevlistHeader(&response_buf);
    LOG(INFO) << "UsbIpManagerTest: received device list response";
    int count = list_header.numExportedDevices;
    EXPECT_EQ(count, 1);
  }

  // Verify if device attachment is successful.
  {
    auto conn = std::make_unique<ClientSocketConnection>(server_port_);
    ASSERT_TRUE(conn->Start());
    OpReqImport import_req;
    memset(import_req.busID, '\0', 32);
    snprintf(import_req.busID, sizeof(import_req.busID), "%s", "1-1");
    import_req.header.version = 2;
    import_req.header.command = OP_REQ_IMPORT_CMD;
    import_req.header.status = 0;
    SmartBuffer request_buf = PackOpReqImport(import_req);
    LOG(INFO) << "UsbIpManagerTest: send import command";
    conn->Send(request_buf);
    SmartBuffer response_buf = conn->Receive(sizeof(OpHeader));
    OpHeader header = UnpackOpHeader(&response_buf);
    EXPECT_EQ(header.status, 0);
  }

  // Verify device deregistration(aka unbind).
  {
    auto conn = std::make_unique<ClientSocketConnection>(server_port_);
    ASSERT_TRUE(conn->Start());
    EXPECT_GT(conn->FD(), 0);
    // Test OP_REQ_BIND_CMD request.
    BindOrUnbindRequest request;
    request.header.version = 2;
    request.header.command = OP_REQ_UNBIND_CMD;
    request.port = 3245;
    SmartBuffer request_buf = PackBindOrUnbindRequest(request);
    LOG(INFO) << "UsbIpManagerTest: send Unbind command";
    conn->Send(request_buf);

    SmartBuffer response_buf = conn->Receive(sizeof(OpHeader));
    OpHeader header = UnpackOpHeader(&response_buf);
    LOG(INFO) << "UsbIpManagerTest: received unbind response "
              << (header.status ? "failure" : "success");
    EXPECT_EQ(header.status, 0);
  }

  // Verify registered device count is back to 0.
  {
    auto conn = std::make_unique<ClientSocketConnection>(server_port_);
    ASSERT_TRUE(conn->Start());
    OpHeader header;
    header.version = 2;
    header.command = OP_REQ_DEVLIST_CMD;
    header.status = 0;
    SmartBuffer request_buf = PackOpHeader(header);
    LOG(INFO) << "UsbIpManagerTest: send device list command";
    conn->Send(request_buf);

    SmartBuffer response_buf = conn->Receive(sizeof(OpRepDevlistHeader));
    OpRepDevlistHeader list_header = UnpackOpRepDevlistHeader(&response_buf);
    LOG(INFO) << "UsbIpManagerTest: received device list response";
    int count = list_header.numExportedDevices;
    EXPECT_EQ(count, 0);
  }
}
