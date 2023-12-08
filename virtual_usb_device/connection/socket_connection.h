// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_SOCKET_CONNECTION_H_
#define VIRTUAL_USB_DEVICE_SOCKET_CONNECTION_H_

#include <memory>
#include <cstdint>

#include "connection.h"

class ClientSocketConnection : public IConnection {
 public:
  ClientSocketConnection(uint16_t port);
  ClientSocketConnection(int fd);

  bool Start() override;
  void Stop() override;

  int FD() const override;
  uint16_t get_port() const override;

  bool Send(const SmartBuffer& smart_buffer) const override;
  SmartBuffer Receive(size_t size) const override;

 private:
  // Listen port of server this client will connect to.
  uint16_t port_;
  // Socket file descriptor.
  int fd_;
};

class ServerSocketConnection : public IServerConnection {
 public:
  ServerSocketConnection(uint16_t port);

  bool Start() override;
  void Stop() override;

  uint16_t get_port() const override;

  std::unique_ptr<IConnection> Accept() override;

 private:
  // Listen port.
  uint16_t port_;
  // File descriptor.
  int fd_;
};

#endif
