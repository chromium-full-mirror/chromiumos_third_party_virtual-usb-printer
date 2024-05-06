// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip_manager.h"

#include <arpa/inet.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

#include <base/functional/bind.h>
#include <base/synchronization/waitable_event.h>
#include <base/task/sequenced_task_runner.h>
#include <base/task/thread_pool.h>
#include <base/threading/thread.h>

#include "usbip_server.h"
#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/virtual_usb_device/op_commands.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/virtual_usb_device/usbip.h"
#include "virtual-usb-printer/virtual_usb_device/usbip_constants.h"

UsbIpManager::UsbIpManager() {
  connection_thread_.reset(new base::Thread("ConnectionThread"));
  base::Thread::Options options(base::ThreadType::kDefault);
  connection_thread_->StartWithOptions(std::move(options));

  core_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
}

UsbIpManager::~UsbIpManager() {
  base::WaitableEvent ev;
  connection_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&UsbIpManager::CleanRequestThreads,
                                base::Unretained(this), &ev));
  ev.Wait();
  LOG(INFO) << "UsbIpManager is getting destructed";
}

void UsbIpManager::Run(const uint16_t port) {
  server_.reset(new UsbIpServer(port));
  server_->SetConnectionHandler(this);
  server_->Start();
}

void UsbIpManager::Stop() {
  if (server_)
    server_->Stop();
}

void UsbIpManager::HandleConnection(std::unique_ptr<IConnection> conn) {
  connection_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&UsbIpManager::HandleInConnectionThread,
                                base::Unretained(this), std::move(conn)));
}

void UsbIpManager::HandleInConnectionThread(std::unique_ptr<IConnection> conn) {
  std::unique_ptr<base::Thread> request_thread(
      new base::Thread("RequestThread"));
  int conn_fd = conn->FD();
  base::Thread::Options options(base::ThreadType::kDefault);

  request_thread->StartWithOptions(std::move(options));
  request_thread->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&UsbIpManager::HandleUsbIpRequest,
                                base::Unretained(this), std::move(conn)));
  request_threads_map_[conn_fd] = std::move(request_thread);
  LOG(INFO) << "New request thread added for connection fd:" << conn_fd;
}

void UsbIpManager::HandleUsbIpRequest(std::unique_ptr<IConnection> conn) {
  SmartBuffer buf = conn->Receive(sizeof(OpHeader));
  if (buf.size() != sizeof(OpHeader)) {
    LOG(INFO) << "An unrecognised message was received - ignoring it";
    DisposeConnection(std::move(conn));
    return;
  }

  OpHeader request = UnpackOpHeader(&buf);
  if (request.command == OP_REQ_BIND_CMD) {
    HandleBind(std::move(conn));
    return;
  }

  if (request.command == OP_REQ_UNBIND_CMD) {
    HandleUnBind(std::move(conn));
    return;
  }

  if (request.command == OP_REQ_DEVLIST_CMD) {
    HandleDeviceList(std::move(conn));
    return;
  }

  if (request.command == OP_REQ_IMPORT_CMD) {
    HandleAttach(std::move(conn));
    return;
  }

  LOG(WARNING) << "Unknown usbip command";

  return;
}

void UsbIpManager::RespondBind(IConnection* conn, bool status) {
  OpHeader response;
  SetOpHeader(OP_REP_BIND_CMD, (status ? 0 : 1 /*error*/), &response);
  SmartBuffer res = PackOpHeader(response);
  conn->Send(res);
}

void UsbIpManager::OnRegisterationDone(std::unique_ptr<IConnection> conn,
                                       uint16_t port,
                                       bool status) {
  LOG(INFO) << "USB Device (port:" << port << ")"
            << " registration : " << (status ? "Success" : "Failed");
  RespondBind(conn.get(), status);
  if (task_finished_cb_)
    task_finished_cb_.Run(conn.get());

  DisposeConnection(std::move(conn));
}

void UsbIpManager::OnDeregisterationDone(std::unique_ptr<IConnection> conn,
                                         uint16_t port,
                                         bool status) {
  LOG(INFO) << "USB Device (port:" << port << ")"
            << " Deregistered : " << (status ? "Success" : "Failed");

  RespondBind(conn.get(), status);
  if (task_finished_cb_)
    task_finished_cb_.Run(conn.get());
  DisposeConnection(std::move(conn));
}

void UsbIpManager::HandleBind(std::unique_ptr<IConnection> conn) {
  uint16_t port = 0;
  SmartBuffer port_buffer = conn->Receive(sizeof(uint16_t));
  if (port_buffer.size() != sizeof(uint16_t)) {
    LOG(ERROR) << "Failed reading port value";
    DisposeConnection(std::move(conn));
    return;
  }

  memcpy(&port, port_buffer.data(), sizeof(uint16_t));
  port = ntohs(port);

  core_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&UsbIpCore::RegisterDevice, base::Unretained(&core_),
                     port),
      base::BindOnce(&UsbIpManager::OnRegisterationDone, base::Unretained(this),
                     std::move(conn), port));
}

void UsbIpManager::HandleUnBind(std::unique_ptr<IConnection> conn) {
  uint16_t port = 0;
  SmartBuffer port_buffer = conn->Receive(sizeof(uint16_t));
  if (port_buffer.size() != sizeof(uint16_t)) {
    LOG(ERROR) << "Failed reading port value";
    DisposeConnection(std::move(conn));
    return;
  }

  memcpy(&port, port_buffer.data(), sizeof(uint16_t));
  port = ntohs(port);

  core_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&UsbIpCore::DeregisterDevice, base::Unretained(&core_),
                     port),
      base::BindOnce(&UsbIpManager::OnDeregisterationDone,
                     base::Unretained(this), std::move(conn), port));
}

void UsbIpManager::HandleUsbRequest(std::unique_ptr<IConnection> conn,
                                    bool last_status) {
  // Either attach or a task to run usb command are finished.
  if (task_finished_cb_)
    task_finished_cb_.Run(conn.get());
  // If last command was successful then continue with processing next command.
  if (last_status) {
    SmartBuffer received = conn->Receive(sizeof(UsbipCmdSubmit));

    if (received.size() == sizeof(UsbipCmdSubmit)) {
      UsbipCmdSubmit command = UnpackUsbipCmdSubmit(&received);
      if (command.header.command == COMMAND_USBIP_CMD_SUBMIT) {
        PrintUsbipCmdSubmit(command);
        core_task_runner_->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(&UsbIpCore::HandleUsbCommandSubmit,
                           base::Unretained(&core_), conn.get(), command),
            base::BindOnce(&UsbIpManager::HandleUsbRequest,
                           base::Unretained(this), std::move(conn)));
      } else if (command.header.command == COMMAND_USBIP_CMD_UNLINK) {
        LOG(INFO) << "Received unlink URB...ignoring";
        // Continue processing next usb request.
        HandleUsbRequest(std::move(conn), true);
      } else {
        LOG(ERROR) << "Unknown usbip command";
      }
      return;
    } else if (received.size() == 0) {
      // Connection is closed by usbip detach <port> command
      core_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&UsbIpCore::DetachUsbDevice,
                                    base::Unretained(&core_), conn->FD()));
      // We can safety disconnect here since above post call only use connection
      // fd.
    } else {
      LOG(ERROR) << "Size of data doesn't match with usbip message";
    }
  }
  DisposeConnection(std::move(conn));
}

void UsbIpManager::HandleAttach(std::unique_ptr<IConnection> conn) {
  core_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&UsbIpCore::HandleAttachInternal, base::Unretained(&core_),
                     conn.get()),
      base::BindOnce(&UsbIpManager::HandleUsbRequest, base::Unretained(this),
                     std::move(conn)));
}

void UsbIpManager::OnHandleDeviceListDone(std::unique_ptr<IConnection> conn) {
  if (task_finished_cb_)
    task_finished_cb_.Run(conn.get());
  DisposeConnection(std::move(conn));
}

void UsbIpManager::HandleDeviceList(std::unique_ptr<IConnection> conn) {
  core_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&UsbIpCore::HandleDeviceListInternal,
                     base::Unretained(&core_), conn.get()),
      base::BindOnce(&UsbIpManager::OnHandleDeviceListDone,
                     base::Unretained(this), std::move(conn)));
}

void UsbIpManager::DisposeConnection(std::unique_ptr<IConnection> conn) {
  connection_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&UsbIpManager::CloseConnection,
                                base::Unretained(this), std::move(conn)));
}

void UsbIpManager::CloseConnection(std::unique_ptr<IConnection> conn) {
  LOG(INFO) << "request thread removed for connection fd:" << conn->FD();
  request_threads_map_.erase(conn->FD());
}

void UsbIpManager::CleanRequestThreads(base::WaitableEvent* event) {
  LOG(INFO) << "Removing all remaining request threads.";
  request_threads_map_.clear();
  event->Signal();
}
