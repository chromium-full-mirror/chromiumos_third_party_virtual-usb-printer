// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip_manager.h"

#include <base/task/thread_pool/thread_pool_instance.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#define USBIP_MANAGER_DEFAULT_TCP_SERV_PORT 3240

constexpr char kUsage[] =
    "UsbIpManager \n"
    "    --port=<unsigned int>";

int main(int argc, char* argv[]) {
  DEFINE_uint32(port, USBIP_MANAGER_DEFAULT_TCP_SERV_PORT,
                "Port that UsbIp server listens to");
  DEFINE_bool(help, false, "print usage");
  brillo::FlagHelper::Init(argc, argv, "Virtual USB Device");
  if (FLAGS_help) {
    LOG(INFO) << "\n" << kUsage;
    return -1;
  }
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  // Create a threadpool instance for this process
  base::ThreadPoolInstance::Create("UsbIpHost Threadpool");
  constexpr size_t max_num_foreground_threads = 8;  // Chosen randomly.
  base::ThreadPoolInstance::InitParams init_params(max_num_foreground_threads);
  base::ThreadPoolInstance::Get()->Start(init_params);

  UsbIpManager host;
  host.Run(FLAGS_port);

  return 0;
}
