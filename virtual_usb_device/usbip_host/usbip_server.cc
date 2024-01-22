// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip_server.h"

#include <arpa/inet.h>
#include <memory>
#include <utility>

#include <base/task/thread_pool.h>
#include <base/threading/thread.h>

#include "usbip_manager.h"

UsbIpServer::UsbIpServer(UsbIpManager* manager, int port)
    : Server(port), manager_(manager) {}

void UsbIpServer::HandleConnection(std::unique_ptr<IConnection> conn) {
  manager_->HandleConnection(std::move(conn));
}

void UsbIpServer::OnServerStarted() {
  printf("USBIPHost::READY\n");
}
