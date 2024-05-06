// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip_server.h"

#include <cstdint>
#include <cstdio>

#include "virtual-usb-printer/virtual_usb_device/server.h"

UsbIpServer::UsbIpServer(const uint16_t port) : Server(port) {}

void UsbIpServer::OnServerStarted() {
  printf("USBIPHost::READY\n");
}
