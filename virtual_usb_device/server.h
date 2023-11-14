// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_SERVER_H_
#define VIRTUAL_USB_DEVICE_SERVER_H_

#include <netinet/in.h>
#include <sys/socket.h>

#include <base/files/scoped_file.h>

#include "smart_buffer.h"

// Abstraction over socket::send which send SmartBuffer in chunks.
void SendBuffer(int sockfd, const SmartBuffer& smart_buffer);
// Abstraction over socket::recv which receives buffers in chunks.
SmartBuffer ReceiveBuffer(int sockfd, size_t size);

// Generic TCP/IP server.
// TODO(santo): Move socket related code in seperate class
// so that it this class could be mocked on unit tests.
class Server {
 public:
  // Creates a TCP server instance which will listen on specified
  // port number when started.
  explicit Server(uint16_t port);
  virtual ~Server() = default;

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  // Creates a socket, binds it to the port and then starts listening.
  // Returns whether the server was started successfully or not.
  bool Start();
  // Shutdown the server and close listening socket.
  void Stop();

  // This function is called after server started to listen.
  // Called from `Start` function.
  virtual void OnServerStarted() {}
  // Processes all incoming message on connected socket.
  virtual void HandleConnection(base::ScopedFD connection) = 0;

  // Returns the port number on which this server is listening.
  uint16_t GetListenPort() const { return port_; }

 private:
  // Creates a socket and start listening for connections.
  bool Listen();
  // Loops and accept connections.
  void HandleLoop();

  uint16_t port_;
  base::ScopedFD server_fd_;
};

#endif  // VIRTUAL_USB_DEVICE_SERVER_H_
