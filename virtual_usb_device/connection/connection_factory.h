// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_CONNECTION_FACTORY_H_
#define VIRTUAL_USB_DEVICE_CONNECTION_FACTORY_H_

#include <memory>
#include <cstdint>

#include "connection.h"

struct ConnectionFactory {
  static ConnectionFactory& instance();

  // Create client connection which can connect to server port `port`.
  std::unique_ptr<IConnection> MakeClientConnection(uint16_t port);

  // Create server connection listening on port `port`.
  std::unique_ptr<IServerConnection> MakeServerConnection(uint16_t port);
};

#endif  // VIRTUAL_USB_DEVICE_CONNECTION_FACTORY_H_
