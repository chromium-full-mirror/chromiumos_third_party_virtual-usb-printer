// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "connection_factory.h"

#include <memory>

#include "socket_connection.h"

ConnectionFactory& ConnectionFactory::instance() {
  static ConnectionFactory s;
  return s;
}  // instance

std::unique_ptr<IConnection> ConnectionFactory::MakeClientConnection(
    uint16_t port) {
  return std::make_unique<ClientSocketConnection>(port);
}

std::unique_ptr<IServerConnection> ConnectionFactory::MakeServerConnection(
    uint16_t port) {
  return std::make_unique<ServerSocketConnection>(port);
}
