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
#include <base/logging.h>

Server::Server(uint16_t port)
    : connection_(ConnectionFactory::instance().MakeServerConnection(port)) {}

void Server::Stop() {
  connection_->Stop();
}

void Server::HandleLoop() {
  while (true) {
    auto conn = connection_->Accept();
    LOG(INFO) << "Connection accepted";
    HandleConnection(std::move(conn));
  }
}

bool Server::Start() {
  if (!connection_->Start())
    return false;

  OnServerStarted();
  HandleLoop();
  return true;
}

void SendBuffer(int sockfd, const SmartBuffer& smart_buffer) {
  size_t remaining = smart_buffer.size();
  size_t total = 0;
  while (remaining > 0) {
    size_t to_send = std::min<size_t>(remaining, 512UL);
    ssize_t sent =
        send(sockfd, smart_buffer.data() + total, to_send, MSG_NOSIGNAL);
    CHECK_GE(sent, 0) << "Failed to write data to socket";
    CHECK_GE(remaining, sent) << "Sent more data than expected";
    size_t sent_unsigned = static_cast<size_t>(sent);
    total += sent_unsigned;
    remaining -= sent_unsigned;
  }
}

SmartBuffer ReceiveBuffer(int sockfd, size_t size) {
  SmartBuffer smart_buffer(size);
  auto buf = std::make_unique<uint8_t[]>(size);
  size_t remaining = size;
  size_t total = 0;
  while (total < size) {
    size_t to_receive = std::min<size_t>(remaining, 512UL);
    ssize_t received = recv(sockfd, buf.get() + total, to_receive, 0);
    if (received == 0 && size != 0) {
      LOG(INFO) << "Client has closed connection";
      return smart_buffer;
    }
    CHECK_GE(remaining, received) << "Received more data than expected";
    size_t received_unsigned = static_cast<size_t>(received);
    total += received_unsigned;
    remaining -= received_unsigned;
  }
  smart_buffer.Add(buf.get(), size);
  return smart_buffer;
}
