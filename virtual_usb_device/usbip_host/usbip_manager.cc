// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip_manager.h"

#include <string>
#include <memory>
#include <utility>

#include <base/strings/stringprintf.h>
#include <base/task/thread_pool.h>

#include "usbip_server.h"
#include "virtual-usb-printer/virtual_usb_device/op_commands.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"
#include "virtual-usb-printer/virtual_usb_device/usbip.h"

UsbIpManager::UsbIpManager(uint16_t port)
    : server_(new UsbIpServer(this, port)) {
  core_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
}

UsbIpManager::~UsbIpManager() {}

void UsbIpManager::Run() {
  connection_thread_.reset(new base::Thread("ConnectionThread"));
  base::Thread::Options options(base::ThreadType::kDefault);
  connection_thread_->StartWithOptions(std::move(options));

  server_->Start();
}

void UsbIpManager::Stop() {
  server_->Stop();
  connection_thread_->Stop();
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
}

void UsbIpManager::HandleUsbIpRequest(std::unique_ptr<IConnection> conn) {
  SmartBuffer buf = conn->Receive(sizeof(OpHeader));
  if (buf.size() != sizeof(OpHeader)) {
    LOG(INFO) << "An unrecognised message was received - ignoring it";
    return;
  }

  OpHeader request = UnpackOpHeader(&buf);
  if (request.command == OP_REQ_BIND_CMD) {
    HandleBind(std::move(conn));
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

void UsbIpManager::OnHandleBindInternalDone(std::unique_ptr<IConnection> conn,
                                            uint16_t port,
                                            bool status) {
  LOG(INFO) << "USB Device (port:" << port << ")"
            << "registeration : " << (status ? "Success" : "Failed");
  RespondBind(conn.get(), status);
  if (status) {
    // We need to track when device process is dead, so we call `conn->Receive`.
    // This call will block since there is no data to receive but returns when
    // device process is dead.
    SmartBuffer status_buffer = conn->Receive(sizeof(uint16_t));
    if (status_buffer.size() == 0) {
      core_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&UsbIpCore::DeregisterDevice,
                                    base::Unretained(&core_), port));
    }
  }

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
      base::BindOnce(&UsbIpCore::HandleBindInternal, base::Unretained(&core_),
                     conn.get(), port),
      base::BindOnce(&UsbIpManager::OnHandleBindInternalDone,
                     base::Unretained(this), std::move(conn), port));
}

bool UsbIpManager::HandleUsbRequest(IConnection* conn) {
  SmartBuffer received = conn->Receive(sizeof(UsbipCmdSubmit));
  if (received.size() == 0) {
    // Connection is closed by usbip detach <port> command
    core_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(&UsbIpCore::DetachUsbDevice,
                                               base::Unretained(&core_), conn));
    return false;
  }

  if (received.size() != sizeof(UsbipCmdSubmit))
    return false;

  UsbipCmdSubmit command = UnpackUsbipCmdSubmit(&received);
  if (command.header.command == COMMAND_USBIP_CMD_SUBMIT) {
    PrintUsbipCmdSubmit(command);
    core_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UsbIpCore::HandleUsbCommandSubmit,
                                  base::Unretained(&core_), conn, command));
    return true;
  }

  if (command.header.command == COMMAND_USBIP_CMD_UNLINK) {
    LOG(INFO) << "Received unlink URB...ignoring";
    return true;
  }

  return false;
}

void UsbIpManager::OnHandleAttachDone(std::unique_ptr<IConnection> conn,
                                      bool status) {
  while (status) {
    status = HandleUsbRequest(conn.get());
  }

  DisposeConnection(std::move(conn));
}

void UsbIpManager::HandleAttach(std::unique_ptr<IConnection> conn) {
  core_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&UsbIpCore::HandleAttachInternal, base::Unretained(&core_),
                     conn.get()),
      base::BindOnce(&UsbIpManager::OnHandleAttachDone, base::Unretained(this),
                     std::move(conn)));
}

void UsbIpManager::OnHandleDeviceListDone(std::unique_ptr<IConnection> conn) {
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
  request_threads_map_.erase(conn->FD());
}
