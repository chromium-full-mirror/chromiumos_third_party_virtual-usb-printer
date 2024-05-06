// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_CONNECTION_CONNECTION_H_
#define VIRTUAL_USB_DEVICE_CONNECTION_CONNECTION_H_

#include <cstdint>
#include <memory>

#include "virtual-usb-printer/common/smart_buffer.h"

// Connection interface - both server and client connections
// implement this interface.
class IConnection {
 public:
  IConnection() = default;
  virtual ~IConnection() = default;

  virtual bool Start() = 0;
  virtual void Stop() = 0;

  // Get server's listen port / client's server listen port.
  virtual uint16_t GetPort() const = 0;

  // Integer descriptor.
  virtual int FD() const = 0;

  // Send data stored in `smart_buffer`.
  // Return true if all bytes in `smart_buffer` are sent, otherwise false.
  virtual bool Send(const SmartBuffer& smart_buffer) const = 0;

  // Receive `size` bytes and return stored in SmartBuffer.
  // Return SmartBuffer with size < `size` if error occurred, otherwise
  // return SmartBuffer with size == `size`.
  virtual SmartBuffer Receive(size_t size) const = 0;
};

// Server connection interface.
class IServerConnection {
 public:
  virtual ~IServerConnection() = default;

  virtual bool Start() = 0;
  virtual void Stop() = 0;

  // Get server's listen port / client's server listen port.
  virtual uint16_t GetPort() const = 0;

  // Accept client "connection" and return unique ptr to
  // resulting IConnection object.
  virtual std::unique_ptr<IConnection> Accept() = 0;
};

#endif  // VIRTUAL_USB_DEVICE_CONNECTION_CONNECTION_H_
