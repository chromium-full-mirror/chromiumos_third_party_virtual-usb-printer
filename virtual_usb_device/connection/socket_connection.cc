// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "socket_connection.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>

#include <base/check_op.h>
#include <base/logging.h>

#include "virtual-usb-printer/common/smart_buffer.h"

ClientSocketConnection::ClientSocketConnection(uint16_t port)
    : port_(port), fd_(-1) {}
ClientSocketConnection::ClientSocketConnection(int fd) : fd_(fd) {}

ClientSocketConnection::~ClientSocketConnection() {
  Stop();
}

int ClientSocketConnection::FD() const {
  return fd_;
}

bool ClientSocketConnection::Start() {
  struct sockaddr_in serv_addr;
  int fd;
  // Prepare for client connection to peer.
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port_);

  // Convert IPv4 and IPv6 addresses from text to binary
  // form.
  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
    LOG(ERROR) << "Invalid peer address/ Peer address not supported";
    return false;
  }
  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    LOG(ERROR) << "\n Socket creation error \n";
    return false;
  }
  int status = connect(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
  if (status < 0) {
    LOG(ERROR) << "\nConnection Failed";
    return false;
  }

  fd_ = fd;
  return true;
}

void ClientSocketConnection::Stop() {
  int status = close(fd_);
  if (status < 0) {
    LOG(ERROR) << "\nClosing connection Failed";
  }
}

bool ClientSocketConnection::Send(const SmartBuffer& smart_buffer) const {
  size_t remaining = smart_buffer.size();
  size_t total = 0;
  while (remaining > 0) {
    size_t to_send = std::min<size_t>(remaining, 512UL);
    ssize_t sent =
        send(fd_, smart_buffer.data() + total, to_send, MSG_NOSIGNAL);
    if (sent == -1) {
      LOG(ERROR) << "Failed to write data to socket";
      return false;
    }
    remaining -= static_cast<size_t>(sent);
    total += static_cast<size_t>(sent);
  }
  return true;
}

SmartBuffer ClientSocketConnection::Receive(size_t size) const {
  SmartBuffer smart_buffer(size);
  std::array<uint8_t, 512> buf;
  size_t remaining = size;
  size_t total = 0;
  while (total < size) {
    size_t to_receive = std::min<size_t>(remaining, 512UL);
    ssize_t received = recv(fd_, buf.data(), to_receive, 0);
    if (received <= 0 && size != 0) {
      LOG(ERROR) << "Client has closed connection";
      return SmartBuffer();
    }
    // Save a received chunk to buf.data().
    size_t received_unsigned = static_cast<size_t>(received);
    smart_buffer.Add(buf.data(), received_unsigned);
    total += received_unsigned;
    remaining -= received_unsigned;
  }
  return smart_buffer;
}

uint16_t ClientSocketConnection::GetPort() const {
  return port_;
}

ServerSocketConnection::ServerSocketConnection(uint16_t port)
    : port_(port), fd_(-1) {}

ServerSocketConnection::~ServerSocketConnection() {
  if (close(fd_) < 0)
    LOG(ERROR) << "Closing listening socket Failed";
}

bool ServerSocketConnection::Start() {
  // 1. create socket.
  fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (fd_ < 0) {
    LOG(ERROR) << "Socket error: " << (errno);
    return false;
  }

  int reuse = 1;
  if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    LOG(ERROR) << "setsockopt(SO_REUSEADDR) failed";
    return false;
  }

  // 2. bind socket.
  sockaddr_in server;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(port_);

  sockaddr* server_socket = reinterpret_cast<sockaddr*>(&server);
  int status = bind(fd_, server_socket, sizeof(server));
  if (status < 0) {
    printf("Bind error : %s\n", strerror(errno));
    return false;
  }

  char address[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &server.sin_addr, address, INET_ADDRSTRLEN)) {
    LOG(INFO) << "Bound server to address " << address << ":"
              << ntohs(server.sin_port);
  } else {
    LOG(ERROR) << "inet_ntop: " << (errno);
    return false;
  }

  status = listen(fd_, SOMAXCONN);
  if (status < 0) {
    LOG(ERROR) << "Listen error: " << (errno) << " status: " << status;
    return false;
  }

  return true;
}

void ServerSocketConnection::Stop() {
  if (fd_ < 0)
    return;
  if (shutdown(fd_, SHUT_RDWR) < 0) {
    int er = errno;
    LOG(ERROR) << "Server shutdown failed with error: " << strerror(er)
               << " for fd " << fd_;
  }
  LOG(INFO) << "Server socket closed on port " << this->port_;
}

uint16_t ServerSocketConnection::GetPort() const {
  return port_;
}

std::unique_ptr<IConnection> ServerSocketConnection::Accept() {
  if (fd_ < 0)
    return nullptr;
  sockaddr_in client;
  socklen_t client_length = sizeof(client);
  int connection =
      accept(fd_, reinterpret_cast<sockaddr*>(&client), &client_length);
  if (connection < 0) {
    int er = errno;
    LOG(ERROR) << "Accept error: " << strerror(er) << " for fd " << fd_;
    return nullptr;
  }
  LOG(INFO) << "Connection address: " << inet_ntoa(client.sin_addr) << ":"
            << client.sin_port;
  return std::make_unique<ClientSocketConnection>(connection);
}
