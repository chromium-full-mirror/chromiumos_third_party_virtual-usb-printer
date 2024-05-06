// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "server.h"

#include <cstdint>
#include <memory>
#include <utility>

#include <base/logging.h>

#include "virtual-usb-printer/virtual_usb_device/connection/socket_connection.h"

Server::Server(uint16_t port) : connection_(new ServerSocketConnection(port)) {}

void Server::Stop() {
  connection_->Stop();
}

void Server::HandleLoop() {
  while (true) {
    auto conn = connection_->Accept();
    if (!conn)
      return;
    LOG(INFO) << "Connection accepted";
    HandleConnection(std::move(conn));
  }
}

void Server::HandleConnection(std::unique_ptr<IConnection> conn) {
  if (connection_handler_)
    connection_handler_->HandleConnection(std::move(conn));
}

bool Server::Start() {
  if (!connection_->Start())
    return false;

  OnServerStarted();
  HandleLoop();
  return true;
}
