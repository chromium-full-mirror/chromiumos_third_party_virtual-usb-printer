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

namespace {

base::ScopedFD SetupServerSocket() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    LOG(ERROR) << "Socket error: " << (errno);
    exit(1);
  }

  int reuse = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    LOG(ERROR) << "setsockopt(SO_REUSEADDR) failed";
  }

  return base::ScopedFD(fd);
}

// Binds the server socket described by `fd` to an address and returns the
// resulting sockaddr_in struct which contains the address.
sockaddr_in BindServerSocket(const base::ScopedFD& sockfd, uint32_t port) {
  sockaddr_in server;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(port);

  sockaddr* server_socket = reinterpret_cast<sockaddr*>(&server);
  int status = bind(sockfd.get(), server_socket, sizeof(server));
  if (status < 0) {
    printf("Bind error : %s\n", strerror(errno));
    exit(1);
  }

  return server;
}

// Accepts a new connection to the server described by `fd` and returns the file
// descriptor of the connection.
base::ScopedFD AcceptConnection(base::ScopedFD& fd) {
  sockaddr_in client;
  socklen_t client_length = sizeof(client);
  int connection =
      accept(fd.get(), reinterpret_cast<sockaddr*>(&client), &client_length);
  if (connection < 0) {
    printf("Accept error :%s\n", strerror(errno));
    LOG(ERROR) << "Accept error: " << errno;
    exit(1);
  }
  LOG(INFO) << "Connection address: " << inet_ntoa(client.sin_addr) << ":"
            << client.sin_port;
  return base::ScopedFD(connection);
}

// Converts the bound address in `server` and reports it in a human-readable
// format.
void ReportBoundAddress(sockaddr_in* server) {
  char address[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &server->sin_addr, address, INET_ADDRSTRLEN)) {
    LOG(INFO) << "Bound server to address " << address << ":"
              << ntohs(server->sin_port);
  } else {
    LOG(ERROR) << "inet_ntop: " << (errno);
    exit(1);
  }
}

}  // namespace

Server::Server(uint16_t port) : port_(port) {}

void Server::Stop() {
  if (shutdown(server_fd_.get(), SHUT_RDWR) < 0) {
    LOG(ERROR) << "Server shutdown failed";
  }
  LOG(INFO) << "Server listening on port:" << port_ << " Closed";
}

bool Server::Listen() {
  server_fd_ = SetupServerSocket();
  sockaddr_in server = BindServerSocket(server_fd_, port_);
  ReportBoundAddress(&server);

  int status = listen(server_fd_.get(), SOMAXCONN);
  if (status < 0) {
    LOG(ERROR) << "Listen error: " << (errno) << " status: " << status;
    return false;
  }

  return true;
}

void Server::HandleLoop() {
  while (true) {
    base::ScopedFD connection = AcceptConnection(server_fd_);
    LOG(INFO) << "Connection accepted";
    HandleConnection(std::move(connection));
  }
}

bool Server::Start() {
  if (!Listen())
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
