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
#include <utility>
#include <vector>

#include <base/synchronization/waitable_event.h>
#include <base/task/thread_pool/thread_pool_instance.h>
#include <base/test/task_environment.h>
#include <gtest/gtest.h>

#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/virtual_usb_device/connection/connection.h"
#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/op_commands.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/usb_device.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/virtual_usb_device/usbip.h"
#include "virtual-usb-printer/virtual_usb_device/usbip_constants.h"

class FakeServerConnection : public IConnection {
 public:
  FakeServerConnection() {
    // Each connection must have unique fd.
    counter++;
    conn_fd_ = counter + 3246;
  }

  // ignore these
  bool Start() override { return true; }
  void Stop() override {}
  uint16_t GetPort() const override { return 3240; }
  int FD() const override { return conn_fd_; }

  SmartBuffer Receive(size_t size) const override {
    size = std::min(size, server_receive_data_.size());
    SmartBuffer r;
    r.Add(server_receive_data_.data(), size);
    server_receive_data_.erase(server_receive_data_.begin(),
                               server_receive_data_.begin() + size);
    return r;
  }

  bool Send(const SmartBuffer& smart_buffer) const override {
    server_send_data_.insert(server_send_data_.end(),
                             smart_buffer.contents().begin(),
                             smart_buffer.contents().end());
    return true;
  }

  SmartBuffer TransferSendBuffer(void) {
    SmartBuffer r;
    r.Add(server_send_data_.data(), server_send_data_.size());
    server_send_data_.clear();
    return r;
  }

  // Stores data sent and receive by Host server
  mutable std::vector<uint8_t> server_receive_data_;
  mutable std::vector<uint8_t> server_send_data_;
  int conn_fd_;
  static size_t counter;
};

size_t FakeServerConnection::counter = 0;

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

  // Sends the urb request to the usb device (which runs in different process)
  // for handling and returns the size of response.
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

class UsbIpManagerTest : public testing::Test {
 public:
  UsbIpManagerTest() = default;
  ~UsbIpManagerTest() = default;
  void TaskFinished(IConnection* conn) {
    FakeServerConnection* fc = dynamic_cast<FakeServerConnection*>(conn);
    current_response_buf_ = fc->TransferSendBuffer();
    cv_.Signal();
  }

 protected:
  void SetUp() override {
    DeviceProxy::CreateCb callback =
        base::BindRepeating(&StubDeviceProxy::Create);
    host_.SetCreateDeviceProxyCallback(callback);

    base::RepeatingCallback<void(IConnection*)> cb = base::BindRepeating(
        &UsbIpManagerTest::TaskFinished, base::Unretained(this));
    host_.SetTaskFinishedCb(cb);
  }

  SmartBuffer HandleRequest(const SmartBuffer& request) {
    cv_.Reset();
    if (current_response_buf_.size() > 0)
      current_response_buf_.Shrink(0);

    std::unique_ptr<FakeServerConnection> fc(new FakeServerConnection());
    fc->server_receive_data_.resize(request.size());
    std::copy(request.contents().begin(), request.contents().end(),
              fc->server_receive_data_.begin());

    host_.HandleConnection(std::move(fc));
    cv_.Wait();

    return current_response_buf_;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::DEFAULT};

  UsbIpManager host_;
  base::WaitableEvent cv_;
  SmartBuffer current_response_buf_;
};

TEST_F(UsbIpManagerTest, HandleUsbIpRequests) {
  const uint16_t port = 3245;
  // Device registration request.
  {
    BindOrUnbindRequest req;
    req.header.version = 2;
    req.header.command = OP_REQ_BIND_CMD;
    req.header.status = 0;
    req.port = port;
    SmartBuffer request = PackBindOrUnbindRequest(req);
    SmartBuffer response = HandleRequest(request);

    OpHeader res = UnpackOpHeader(&response);
    EXPECT_EQ(res.command, OP_REP_BIND_CMD);  // status 0 means success.
    EXPECT_EQ(res.status, 0);                 // status 0 means success.
  }

  // Test device listing with OP_REQ_DEVLIST_CMD request.
  {
    OpReqDevlist req;
    req.version = 2;
    req.command = OP_REQ_DEVLIST_CMD;
    req.status = 0;
    SmartBuffer request = PackOpHeader(req);
    SmartBuffer response = HandleRequest(request);

    auto list = UnpackOpRepDevlist(&response);
    EXPECT_EQ(list.header.header.command, OP_REP_DEVLIST_CMD);
    EXPECT_EQ(list.header.numExportedDevices, 1);
  }

  // Test attaching of device with OP_REQ_IMPORT_CMD.
  {
    OpReqImport req;
    req.header.version = 2;
    req.header.command = OP_REQ_IMPORT_CMD;
    req.header.status = 0;
    std::fill(req.busID, req.busID + 32, '\0');
    snprintf(req.busID, sizeof(req.busID), "%s", "1-1");
    SmartBuffer request = PackOpReqImport(req);
    SmartBuffer response = HandleRequest(request);

    auto rep = UnpackOpRepImport(&response);
    EXPECT_EQ(rep.header.status, 0);
    EXPECT_EQ(rep.header.command, OP_REP_IMPORT_CMD);
    EXPECT_EQ(strncmp(rep.device.busID, "1-1", sizeof("1-1")), 0);
  }

  // Device deregistration request.
  {
    BindOrUnbindRequest req;
    req.header.version = 2;
    req.header.command = OP_REQ_UNBIND_CMD;
    req.header.status = 0;
    req.port = port;
    SmartBuffer request = PackBindOrUnbindRequest(req);
    SmartBuffer response = HandleRequest(request);

    OpHeader res = UnpackOpHeader(&response);
    EXPECT_EQ(res.status, 0);  // status 0 means success.
  }

  // Test device list count is back to zero
  {
    OpReqDevlist req;
    req.version = 2;
    req.command = OP_REQ_DEVLIST_CMD;
    req.status = 0;
    SmartBuffer request = PackOpHeader(req);
    SmartBuffer response = HandleRequest(request);

    auto list = UnpackOpRepDevlist(&response);
    EXPECT_EQ(list.header.header.command, OP_REP_DEVLIST_CMD);
    EXPECT_EQ(list.header.numExportedDevices, 0);
  }
}
