// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip_core.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <base/functional/bind.h>
#include <gtest/gtest.h>

#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/virtual_usb_device/connection/connection.h"
#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/usb_device/usb_device.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/virtual_usb_device/usbip.h"
#include "virtual-usb-printer/virtual_usb_device/usbip_constants.h"
#include "virtual-usb-printer/virtual_usb_device/usbip_host/device_proxy.h"

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

// A DeviceProxy class implementation that communicates with
// usb device using direct function call.
class StubDeviceProxy : public DeviceProxy {
 public:
  static std::unique_ptr<DeviceProxy> Create(const uint16_t port,
                                             const ::BusId& id,
                                             const size_t devnum) {
    std::unique_ptr<StubDeviceProxy> proxy(
        new StubDeviceProxy(port, id, devnum));
    return proxy;
  }

  ~StubDeviceProxy() override = default;

  bool ConnectDevice() override { return true; }

  // Gets the result of usb request by making direct function call
  // to `UsbDevice`
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

  std::unique_ptr<UsbDevice> usb_device_;
};

class UsbIpCoreTest : public testing::Test {
 protected:
  void SetUp() override {
    core_ = std::make_unique<UsbIpCore>();
    DeviceProxy::CreateCb callback =
        base::BindRepeating(&StubDeviceProxy::Create);
    core_->SetCreateDeviceProxyCallback(callback);
  }

  std::unique_ptr<UsbIpCore> core_;
};

TEST_F(UsbIpCoreTest, RegisterDevice) {
  const uint16_t device_port = 8888;
  bool status = core_->RegisterDevice(device_port);
  EXPECT_EQ(status, true);
  status = core_->DeregisterDevice(device_port);
  EXPECT_EQ(status, true);
}

TEST_F(UsbIpCoreTest, HandleAttachInternalNotExists) {
  const uint16_t device_port = 8888;
  ASSERT_TRUE(core_->RegisterDevice(device_port));

  // Prepare the 32-bytes with busid read by the method from the connection.
  FakeServerConnection fc;
  fc.server_receive_data_.resize(32);
  const std::string busid("1-5");
  std::copy(busid.begin(), busid.end(), fc.server_receive_data_.begin());
  EXPECT_FALSE(core_->HandleAttachInternal(&fc));
  EXPECT_FALSE(core_->IsAttached("1-5"));

  auto buf = fc.TransferSendBuffer();
  EXPECT_EQ(buf.size(), sizeof(OpRepImport));
  auto rep = UnpackOpRepImport(&buf);
  EXPECT_EQ(rep.header.status, -1);
}

TEST_F(UsbIpCoreTest, HandleAttachInternalSuccess) {
  const uint16_t device_port = 8888;
  ASSERT_TRUE(core_->RegisterDevice(device_port));

  // Prepare the 32-bytes with busid read by the method from the connection.
  FakeServerConnection fc;
  std::array<char, 32> busid = {"1-1"};
  fc.server_receive_data_.resize(32);
  std::fill(fc.server_receive_data_.begin(), fc.server_receive_data_.end(), 0);
  std::copy(busid.begin(), busid.end(), fc.server_receive_data_.begin());

  EXPECT_TRUE(core_->HandleAttachInternal(&fc));
  EXPECT_TRUE(core_->IsAttached("1-1"));

  auto buf = fc.TransferSendBuffer();
  EXPECT_EQ(buf.size(), sizeof(OpRepImport));
  auto rep = UnpackOpRepImport(&buf);
  EXPECT_EQ(rep.header.status, 0);

  core_->DetachUsbDevice(fc.FD());
  EXPECT_FALSE(core_->IsAttached("1-1"));
  buf = fc.TransferSendBuffer();
  EXPECT_EQ(buf.size(), 0);
}

TEST_F(UsbIpCoreTest, HandleDeviceListInternal) {
  const uint16_t device_port = 8888;
  // Register device 0.
  ASSERT_TRUE(core_->RegisterDevice(device_port));

  // Attach device 0.
  FakeServerConnection fc1;
  fc1.server_receive_data_.resize(32);
  const std::string busid("1-1");
  std::copy(busid.begin(), busid.end(), fc1.server_receive_data_.begin());
  EXPECT_TRUE(core_->HandleAttachInternal(&fc1));
  EXPECT_TRUE(core_->IsAttached("1-1"));
  auto buf = fc1.TransferSendBuffer();
  EXPECT_EQ(buf.size(), sizeof(OpRepImport));
  auto rep = UnpackOpRepImport(&buf);
  EXPECT_EQ(rep.header.status, 0);

  // List devices.
  core_->HandleDeviceListInternal(&fc1);
  buf = fc1.TransferSendBuffer();
  auto list = UnpackOpRepDevlist(&buf);
  EXPECT_EQ(list.header.numExportedDevices, 1);

  const uint16_t device_port2 = 8889;
  // Register device 1.
  ASSERT_TRUE(core_->RegisterDevice(device_port2));

  // Attach device 1.
  FakeServerConnection fc2;
  fc2.server_receive_data_.resize(32);
  const std::string busid1("1-2");
  std::copy(busid1.begin(), busid1.end(), fc2.server_receive_data_.begin());
  EXPECT_TRUE(core_->HandleAttachInternal(&fc2));
  EXPECT_TRUE(core_->IsAttached("1-2"));
  buf = fc2.TransferSendBuffer();
  EXPECT_EQ(buf.size(), sizeof(OpRepImport));
  rep = UnpackOpRepImport(&buf);
  EXPECT_EQ(rep.header.status, 0);

  // List devices.
  core_->HandleDeviceListInternal(&fc2);
  buf = fc2.TransferSendBuffer();
  list = UnpackOpRepDevlist(&buf);
  EXPECT_EQ(list.header.numExportedDevices, 2);

  // Detach device 0.
  core_->DetachUsbDevice(fc1.FD());
  EXPECT_FALSE(core_->IsAttached("1-1"));
  buf = fc1.TransferSendBuffer();
  EXPECT_EQ(buf.size(), 0);

  // Detach device 1.
  core_->DetachUsbDevice(fc2.FD());
  EXPECT_FALSE(core_->IsAttached("1-2"));
  buf = fc2.TransferSendBuffer();
  EXPECT_EQ(buf.size(), 0);
}

TEST_F(UsbIpCoreTest, HandleUsbCommandSubmit_HostToDevice) {
  const uint16_t device_port = 8888;
  ASSERT_TRUE(core_->RegisterDevice(device_port));

  // Prepare the 32-bytes with busid read by the method from the connection.
  FakeServerConnection fc;
  fc.server_receive_data_.resize(32);
  const std::string busid("1-1");
  std::copy(busid.begin(), busid.end(), fc.server_receive_data_.begin());

  EXPECT_TRUE(core_->HandleAttachInternal(&fc));
  EXPECT_TRUE(core_->IsAttached("1-1"));

  auto buf = fc.TransferSendBuffer();
  auto rep = UnpackOpRepImport(&buf);
  EXPECT_EQ(rep.header.status, 0);

  UsbipCmdSubmit cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.header.direction = 0;
  buf = PackUsbipCmdSubmit(cmd);
  fc.server_receive_data_.resize(buf.size());
  memcpy(fc.server_receive_data_.data(), buf.data(), buf.size());
  EXPECT_TRUE(core_->HandleUsbCommandSubmit(&fc, cmd));

  buf = fc.TransferSendBuffer();
  auto rep_submit = UnpackUsbipRetSubmit(&buf);
  EXPECT_EQ(rep_submit.status, 0);
  EXPECT_EQ(rep_submit.header.command, COMMAND_USBIP_RET_SUBMIT);
  EXPECT_EQ(rep_submit.header.direction, rep_submit.header.direction);
  EXPECT_EQ(rep_submit.header.ep, rep_submit.header.ep);

  core_->DetachUsbDevice(fc.FD());
  EXPECT_FALSE(core_->IsAttached("1-1"));
  buf = fc.TransferSendBuffer();
  EXPECT_EQ(buf.size(), 0);
}

TEST_F(UsbIpCoreTest, HandleUsbCommandSubmit_DeviceToHost) {
  const uint16_t device_port = 8888;
  ASSERT_TRUE(core_->RegisterDevice(device_port));

  // Prepare the 32-bytes with busid read by the method from the connection.
  FakeServerConnection fc;
  fc.server_receive_data_.resize(32);
  const std::string busid("1-1");
  std::copy(busid.begin(), busid.end(), fc.server_receive_data_.begin());

  EXPECT_TRUE(core_->HandleAttachInternal(&fc));
  EXPECT_TRUE(core_->IsAttached("1-1"));

  auto buf = fc.TransferSendBuffer();
  auto rep = UnpackOpRepImport(&buf);
  EXPECT_EQ(rep.header.status, 0);

  UsbipCmdSubmit cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.header.direction = 1;
  buf = PackUsbipCmdSubmit(cmd);
  fc.server_receive_data_.resize(buf.size());
  memcpy(fc.server_receive_data_.data(), buf.data(), buf.size());
  EXPECT_TRUE(core_->HandleUsbCommandSubmit(&fc, cmd));

  buf = fc.TransferSendBuffer();
  auto rep_submit = UnpackUsbipRetSubmit(&buf);
  EXPECT_EQ(rep_submit.status, 0);
  EXPECT_EQ(rep_submit.header.command, COMMAND_USBIP_RET_SUBMIT);
  EXPECT_EQ(rep_submit.header.direction, rep_submit.header.direction);
  EXPECT_EQ(rep_submit.header.ep, rep_submit.header.ep);

  core_->DetachUsbDevice(fc.FD());
  EXPECT_FALSE(core_->IsAttached("1-1"));
  buf = fc.TransferSendBuffer();
  EXPECT_EQ(buf.size(), 0);
}
