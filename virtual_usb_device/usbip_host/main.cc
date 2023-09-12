// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip_manager.h"
#include <base/task/thread_pool/thread_pool_instance.h>

int main(int argc, char* argv[]) {
  // Create a threadpool instance for this process.
  base::ThreadPoolInstance::Create("UsbIpHost Threadpool");
  const size_t max_num_foreground_threads = 8;  // Chosen randomly
  base::ThreadPoolInstance::InitParams init_params(max_num_foreground_threads);
  base::ThreadPoolInstance::Get()->Start(init_params);

  UsbIpManager manager{};
  manager.Run();
}
