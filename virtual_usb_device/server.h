// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_SERVER_H_
#define VIRTUAL_USB_DEVICE_SERVER_H_

#include <cstdint>
#include <memory>

#include <base/memory/raw_ptr.h>

#include "virtual-usb-printer/virtual_usb_device/connection/connection.h"

class ConnectionHandler {
 public:
  ConnectionHandler() = default;
  virtual ~ConnectionHandler() = default;

  virtual void HandleConnection(std::unique_ptr<IConnection> conn) = 0;
};

// Generic TCP/IP server.
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
  void HandleConnection(std::unique_ptr<IConnection> conn);

  // Returns the port number on which this server is listening.
  uint16_t GetListenPort() const { return connection_->GetPort(); }

  void SetConnectionHandler(ConnectionHandler* handler) {
    connection_handler_ = handler;
  }
  ConnectionHandler* GetConnectionHandler() const {
    return connection_handler_;
  }

 private:
  // Creates a socket and start listening for connections.
  bool Listen();
  // Loops and accept connections.
  void HandleLoop();

  std::unique_ptr<IServerConnection> connection_;
  base::raw_ptr<ConnectionHandler> connection_handler_;
};

#endif  // VIRTUAL_USB_DEVICE_SERVER_H_
