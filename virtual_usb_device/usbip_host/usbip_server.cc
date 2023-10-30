// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip_server.h"

#include <arpa/inet.h>
#include <utility>

#include <base/task/thread_pool.h>

#include "usbip_manager.h"

UsbIpServer::UsbIpServer(UsbIpManager* manager, int port)
    : Server(port), manager_(manager) {}

void UsbIpServer::HandleConnection(base::ScopedFD connection) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(&UsbIpManager::HandleConnection,
                     base::Unretained(manager_), std::move(connection)));
}
