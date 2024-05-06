// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_MANAGER_H_
#define VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_MANAGER_H_

#include <cstdint>
#include <memory>

#include <base/containers/flat_map.h>
#include <base/memory/scoped_refptr.h>
#include <base/synchronization/waitable_event.h>
#include <base/task/sequenced_task_runner.h>
#include <base/threading/thread.h>

#include "device_proxy.h"
#include "usbip_core.h"
#include "virtual-usb-printer/virtual_usb_device/connection/connection.h"
#include "virtual-usb-printer/virtual_usb_device/server.h"

// This class manages usbip connections and requests recevied from the user.
// Each usbip requests is processed in the new `RequestThread`, which is created
// and managed by `ConnectionThread`. `RequestThreads` are cleaned up once
// requests are serviced. The usbip command are finally handled in `UsbIpCore`
// class.
// This class also owns any connection created by usbip client.
class UsbIpManager : public ConnectionHandler {
 public:
  using TaskFinishCb = base::RepeatingCallback<void(IConnection*)>;

  // Constructs UsbIpManager object.
  UsbIpManager();
  ~UsbIpManager();

  UsbIpManager(const UsbIpManager&) = delete;
  UsbIpManager& operator=(const UsbIpManager&) = delete;

  // Starts the server which listens on usbip port and also launch
  // ConnectionThread for handling socket connections.
  void Run(const uint16_t port);
  // Stops the server and ConnectionThread.
  void Stop();

  // override from ConnectionHandler
  // Loops and process usbip message received on connection.
  void HandleConnection(std::unique_ptr<IConnection> conn) override;

  // Posts the task in `UsbIpCore` task runner to handle `OP_REQ_BIND_CMD` usbip
  // message.
  // Note: OP_REQ_BIND_CMD is custom defined usbip message.
  void HandleBind(std::unique_ptr<IConnection> conn);
  void HandleUnBind(std::unique_ptr<IConnection> conn);
  // Callback for `HandleBind` which is triggered in `RequestThread`.
  // `status` notfies about whether HandleBind was successful or not.
  // `port` is the port on which device is listening.
  void OnRegisterationDone(std::unique_ptr<IConnection> conn,
                           uint16_t port,
                           bool status);
  // Callback for `HandleUnBind` which is triggered in `RequestThread`.
  // `status` notfies about whether HandleBind was successful or not.
  // `port` is the port on which device is listening.
  void OnDeregisterationDone(std::unique_ptr<IConnection> conn,
                             uint16_t port,
                             bool status);

  // Posts a task in the `UsbIpCore` task runner to handle `OP_REQ_DEVLIST`
  // usbip message.
  void HandleDeviceList(std::unique_ptr<IConnection> conn);
  // Callback for `HandleDeviceList` which is triggered in `RequestThread`.
  void OnHandleDeviceListDone(std::unique_ptr<IConnection> conn);

  // Posts a task in the `UsbIpCore` task runner to handle `OP_REQ_IMPORT` usbip
  // message.
  void HandleAttach(std::unique_ptr<IConnection> conn);

  void SetCreateDeviceProxyCallback(DeviceProxy::CreateCb& callback) {
    core_.SetCreateDeviceProxyCallback(callback);
  }

  // Register a Callback which will be triggered when asynchronous usbip task is
  // finished.
  void SetTaskFinishedCb(TaskFinishCb& cb) { task_finished_cb_ = cb; }

 private:
  // Responds with the status of OP_REQ_BIND_CMD usbip message.
  void RespondBind(IConnection* conn, bool status);

  // Reads the usbip submit command from connection and post task in `UsbIpCore`
  // task runner for handling.
  // This function is also used as a callback for task posted on `UsbIpCore`
  // thread and on the callback next usbip command is processed. This ensures
  // that all the commands on a specific connection are handled in the
  // serialized manner, even if multiple threads are involved. `last_status`
  // tells whether previous call to this function was success or not.
  void HandleUsbRequest(std::unique_ptr<IConnection> conn, bool last_status);

  // Posts task in the `ConnectionThread` for processing.
  void HandleInConnectionThread(std::unique_ptr<IConnection> conn);

  // Reads connection `conn`, parses usbip commands (BIND, DEVLIST, IMPORT) and
  // then process it.
  void HandleUsbIpRequest(std::unique_ptr<IConnection> conn);

  // This runs inside `RequestThread` and posts `CloseConnection` task in
  // `ConnectionThread` for closing the connection.
  void DisposeConnection(std::unique_ptr<IConnection> conn);

  // Closes the connection `conn` and removes  thread which was servicing this
  // connection.
  void CloseConnection(std::unique_ptr<IConnection> conn);
  void CleanRequestThreads(base::WaitableEvent* event);

  UsbIpCore core_;

  std::unique_ptr<Server> server_;

  // This thread processes each usbip requests in the new thread and manages
  // their lifetime.
  std::unique_ptr<base::Thread> connection_thread_;
  // Map of live connection fd and it's servicing thread.
  // Entry is removed once connection is closed, thus stopping corresponding
  // thread.
  base::flat_map<int, std::unique_ptr<base::Thread>> request_threads_map_;
  // Task runner where all function of `UsbIpCore` are run in a sequence to
  // ensure all data access are thread safe.
  scoped_refptr<base::SequencedTaskRunner> core_task_runner_;
  // Callback to be triggered when asynchronous usbip task are finished.
  TaskFinishCb task_finished_cb_;
};

#endif  // VIRTUAL_USB_DEVICE_USBIP_HOST_USBIP_MANAGER_H_
