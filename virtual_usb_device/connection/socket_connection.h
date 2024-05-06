// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_CONNECTION_SOCKET_CONNECTION_H_
#define VIRTUAL_USB_DEVICE_CONNECTION_SOCKET_CONNECTION_H_

#include <cstdint>
#include <memory>

#include "connection.h"
#include "virtual-usb-printer/common/smart_buffer.h"

// IP socket client implementation of `IConnection`
class ClientSocketConnection : public IConnection {
 public:
  explicit ClientSocketConnection(uint16_t port);
  explicit ClientSocketConnection(int fd);
  ~ClientSocketConnection() override;

  bool Start() override;
  void Stop() override;

  int FD() const override;
  uint16_t GetPort() const override;

  bool Send(const SmartBuffer& smart_buffer) const override;
  SmartBuffer Receive(size_t size) const override;

 private:
  // Listen port of server this client will connect to.
  uint16_t port_;
  int fd_;
};

// IP socket server implementation of `IConnection`
class ServerSocketConnection : public IServerConnection {
 public:
  explicit ServerSocketConnection(uint16_t port);
  ~ServerSocketConnection();

  bool Start() override;
  void Stop() override;

  uint16_t GetPort() const override;

  std::unique_ptr<IConnection> Accept() override;

 private:
  // Listen port.
  uint16_t port_;
  // File descriptor.
  int fd_;
};

#endif  // VIRTUAL_USB_DEVICE_CONNECTION_SOCKET_CONNECTION_H_
