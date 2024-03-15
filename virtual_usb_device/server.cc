// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "server.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <algorithm>
#include <utility>
#include <memory>

#include <base/check_op.h>
#include <base/functional/bind.h>
#include <base/functional/callback.h>
#include <base/functional/callback_helpers.h>
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
