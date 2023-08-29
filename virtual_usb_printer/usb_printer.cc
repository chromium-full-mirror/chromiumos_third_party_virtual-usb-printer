// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_printer.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <utility>

#include <base/check.h>
#include <base/check_op.h>
#include <base/logging.h>
#include <base/notreached.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "ipp_util.h"
#include "server.h"
#include "usbip_constants.h"
#include "value_util.h"
#include "load_config.h"

namespace {

// Bitmask constants used for extracting individual values out of
// UsbControlRequest which is packed inside of a uint64_t.
const uint64_t REQUEST_TYPE_MASK{0xFFULL << 56};
const uint64_t REQUEST_MASK{0xFFULL << 48};
const uint64_t VALUE_0_MASK{0xFFULL << 40};
const uint64_t VALUE_1_MASK{0xFFULL << 32};
const uint64_t INDEX_0_MASK{0xFFUL << 24};
const uint64_t INDEX_1_MASK{0xFFUL << 16};
const uint64_t LENGTH_MASK{0xFFFFUL};

// Returns the numeric value of the "direction" bit within |bmRequestType|.
int GetControlDirection(uint8_t bmRequestType) {
  // Data transfer direction is bit 7.
  return (bmRequestType >> 7) & 0x01;
}

// Returns the numeric value of the "type" stored within the |bmRequestType|
// bitmap.
int GetControlType(uint8_t bmRequestType) {
  // The "type" of the request is stored within bits 5 and 6 of |bmRequestType|.
  // So we shift these bits down to the least significant bits and perform a
  // bitwise AND operation in order to clear any other bits and return strictly
  // the number value of the type bits.
  return (bmRequestType >> 5) & 3;
}

// Returns the numeric value of the "recipient" stored within |bmRequestType|.
int GetControlRecipient(uint8_t bmRequestType) {
  // The "recipient" of the request is stored in bits 0..4, so we just need to
  // mask it off.
  return bmRequestType & 0x1f;
}

// Bit 7 indicates the data transfer direction:
//   0 = Host to Device
//   1 = Device to Host
std::string RequestDirectionString(uint8_t bmRequestType) {
  return GetControlDirection(bmRequestType) ? "To host" : "To device";
}

std::string RequestTypeString(uint8_t bmRequestType) {
  switch (GetControlType(bmRequestType)) {
    case STANDARD_TYPE:
      return "Standard";
    case CLASS_TYPE:
      return "Class";
    case VENDOR_TYPE:
      return "Vendor";
    case RESERVED_TYPE:
      return "Reserved";
    default:
      NOTREACHED();
  }

  return "Invalid";
}

std::string RequestRecipientString(uint8_t bmRequestType) {
  switch (GetControlRecipient(bmRequestType)) {
    case RECIPIENT_DEVICE:
      return "Device";
    case RECIPIENT_INTERFACE:
      return "Interface";
    case RECIPIENT_ENDPOINT:
      return "Endpoint";
    case RECIPIENT_OTHER:
      return "Other";
    default:
      return "Reserved";
  }
}

std::string StandardDeviceRequestString(uint8_t bRequest) {
  switch (bRequest) {
    case GET_STATUS:
      return "GET_STATUS";
    case CLEAR_FEATURE:
      return "CLEAR_FEATURE";
    case SET_FEATURE:
      return "SET_FEATURE";
    case SET_ADDRESS:
      return "SET_ADDRESS";
    case GET_DESCRIPTOR:
      return "GET_DESCRIPTOR";
    case SET_DESCRIPTOR:
      return "SET_DESCRIPTOR";
    case GET_CONFIGURATION:
      return "GET_CONFIGURATION";
    case SET_CONFIGURATION:
      return "SET_CONFIGURATION";
    case GET_INTERFACE:
      return "GET_INTERFACE";
    case SET_INTERFACE:
      return "SET_INTERFACE";
    case SET_FRAME:
      return "SET_FRAME";
    default:
      LOG(ERROR) << "Unknown standard device bRequest value "
                 << base::StringPrintf("0x%02X", bRequest);
      return base::StringPrintf("BREQUEST_RAW_%02X", bRequest);
  }
}

std::string RequestNameString(uint8_t bmRequestType, uint8_t bRequest) {
  if (GetControlType(bmRequestType) == STANDARD_TYPE) {
    switch (GetControlRecipient(bmRequestType)) {
      case RECIPIENT_DEVICE:
        return StandardDeviceRequestString(bRequest);

        // TODO(b/238353195): Add mappings for known device class requests.
    }
  }

  LOG(ERROR) << "No mapping defined for bmRequestType "
             << base::StringPrintf("0x%02X", bmRequestType);
  return base::StringPrintf("%02X", bRequest);
}

std::string DescriptorTypeString(uint8_t wValue) {
  switch (wValue) {
    case USB_DESCRIPTOR_DEVICE:
      return "USB_DESCRIPTOR_DEVICE";
    case USB_DESCRIPTOR_CONFIGURATION:
      return "USB_DESCRIPTOR_CONFIGURATION";
    case USB_DESCRIPTOR_STRING:
      return "USB_DESCRIPTOR_STRING";
    case USB_DESCRIPTOR_INTERFACE:
      return "USB_DESCRIPTOR_INTERFACE";
    case USB_DESCRIPTOR_ENDPOINT:
      return "USB_DESCRIPTOR_ENDPOINT";
    case USB_DESCRIPTOR_DEVICE_QUALIFIER:
      return "USB_DESCRIPTOR_DEVICE_QUALIFIER";
    case USB_DESCRIPTOR_OTHER_SPEED:
      return "USB_DESCRIPTOR_OTHER_SPEED";
    case USB_DESCRIPTOR_INTERFACE_POWER:
      return "USB_DESCRIPTOR_INTERFACE_POWER";
    case USB_DESCRIPTOR_OTG:
      return "USB_DESCRIPTOR_OTG";
    case USB_DESCRIPTOR_DEBUG:
      return "USB_DESCRIPTOR_DEBUG";
    default:
      LOG(ERROR) << "Unknown descriptor type request: "
                 << base::StringPrintf("0x%02X", wValue);
      return base::StringPrintf("USB_DESCRIPTOR_RAW_%02X", wValue);
  }
}

// Unpacks the standard USB SETUP packet contained within |setup| into a
// UsbControlRequest struct and returns the result.
UsbControlRequest CreateUsbControlRequest(int64_t setup) {
  UsbControlRequest request;
  request.bmRequestType = (setup & REQUEST_TYPE_MASK) >> 56;
  request.bRequest = (setup & REQUEST_MASK) >> 48;
  request.wValue0 = (setup & VALUE_0_MASK) >> 40;
  request.wValue1 = (setup & VALUE_1_MASK) >> 32;
  request.wIndex0 = (setup & INDEX_0_MASK) >> 24;
  request.wIndex1 = (setup & INDEX_1_MASK) >> 16;
  request.wLength = ntohs(setup & LENGTH_MASK);

  VLOG(2) << "bmRequestType "
          << base::StringPrintf("0x%02X", request.bmRequestType);
  VLOG(2) << "  direction   " << GetControlDirection(request.bmRequestType)
          << " " << RequestDirectionString(request.bmRequestType);
  VLOG(2) << "  type        " << GetControlType(request.bmRequestType) << " "
          << RequestTypeString(request.bmRequestType);
  VLOG(2) << "  recipient   " << GetControlRecipient(request.bmRequestType)
          << " " << RequestRecipientString(request.bmRequestType);
  VLOG(2) << "bRequest      " << base::StringPrintf("0x%02X", request.bRequest)
          << " " << RequestNameString(request.bmRequestType, request.bRequest);
  VLOG(2) << "wValue0       " << base::StringPrintf("0x%02X", request.wValue0);
  VLOG(2) << "wValue1       " << base::StringPrintf("0x%02X", request.wValue1);
  VLOG(2) << "wIndex0       " << base::StringPrintf("0x%02X", request.wIndex0);
  VLOG(2) << "wIndex1       " << base::StringPrintf("0x%02X", request.wIndex1);
  VLOG(2) << "wLength       " << unsigned{request.wLength};

  return request;
}

// Appends |buf| to the file at |path|. If no file exists at |path|, it is
// created.
void AppendToFile(base::FilePath path, SmartBuffer buf) {
  base::File file(path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE |
                            base::File::FLAG_APPEND);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open/create file at " << path;
    return;
  }

  int written = file.WriteAtCurrentPos(
      reinterpret_cast<const char*>(buf.data()), buf.size());
  if (written != buf.size()) {
    PLOG(ERROR) << "Failed to write buf to file at " << path;
  }
}

}  // namespace

void InterfaceManager::QueueMessage(const SmartBuffer& message) {
  queue_.push(message);
}

bool InterfaceManager::QueueEmpty() const {
  return queue_.empty();
}

std::optional<SmartBuffer> InterfaceManager::AddMessageAndReturnIfComplete(
    const SmartBuffer& message) {
  message_.Add(message);
  const bool complete =
      receiving_chunked_ ? ContainsFinalChunk(message)
                         : message_.size() == request_header_.ContentLength();
  if (!complete) {
    return std::nullopt;
  }

  SmartBuffer payload;
  if (receiving_chunked_) {
    // Assemble the chunks into the HTTP response body.
    payload = MergeDocument(&message_);
  } else {
    payload = message_;
    message_.Erase(0, message_.size());
  }

  receiving_message_ = false;
  return payload;
}

SmartBuffer InterfaceManager::PopMessage() {
  CHECK(!QueueEmpty()) << "Can't pop message from empty queue.";
  auto message = queue_.front();
  queue_.pop();
  return message;
}

// explicit
UsbDescriptors::UsbDescriptors(
    const UsbDeviceDescriptor& device_descriptor,
    const UsbConfigurationDescriptor& configuration_descriptor,
    const UsbDeviceQualifierDescriptor& qualifier_descriptor,
    const std::vector<std::vector<char>>& string_descriptors,
    const std::vector<char>& ieee_device_id,
    const std::vector<UsbInterfaceDescriptor>& interface_descriptors,
    const std::map<uint8_t, std::vector<UsbEndpointDescriptor>>&
        endpoint_descriptors)
    : device_descriptor_(device_descriptor),
      configuration_descriptor_(configuration_descriptor),
      qualifier_descriptor_(qualifier_descriptor),
      string_descriptors_(string_descriptors),
      ieee_device_id_(ieee_device_id),
      interface_descriptors_(interface_descriptors),
      endpoint_descriptors_(endpoint_descriptors) {}

std::optional<UsbDescriptors> UsbDescriptors::Create(
    const std::string& descriptor_file) {
  std::optional<std::string> descriptors_contents =
      GetJSONContents(descriptor_file);
  if (!descriptors_contents.has_value()) {
    LOG(ERROR) << "Failed to load file contents for " << descriptor_file;
    return std::nullopt;
  }

  std::optional<base::Value> descriptors =
      base::JSONReader::Read(*descriptors_contents);
  if (!descriptors) {
    LOG(ERROR) << "Failed to parse " << descriptor_file;
    return std::nullopt;
  }

  if (!descriptors->is_dict()) {
    LOG(ERROR) << "Failed to extract printer configuration as dictionary";
    return std::nullopt;
  }

  UsbDeviceDescriptor device = GetDeviceDescriptor(descriptors->GetDict());
  UsbConfigurationDescriptor configuration =
      GetConfigurationDescriptor(descriptors->GetDict());
  UsbDeviceQualifierDescriptor qualifier =
      GetDeviceQualifierDescriptor(descriptors->GetDict());
  std::vector<UsbInterfaceDescriptor> interfaces =
      GetInterfaceDescriptors(descriptors->GetDict());
  std::map<uint8_t, std::vector<UsbEndpointDescriptor>> endpoint_map =
      GetEndpointDescriptorMap(descriptors->GetDict());
  std::vector<std::vector<char>> strings =
      GetStringDescriptors(descriptors->GetDict());
  std::vector<char> ieee_device_id = GetIEEEDeviceId(descriptors->GetDict());

  return UsbDescriptors(device, configuration, qualifier, strings,
                        ieee_device_id, interfaces, endpoint_map);
}

UsbPrinter::UsbPrinter(const UsbDescriptors& usb_descriptors,
                       const base::FilePath& document_output_path,
                       const base::FilePath& http_output_dir,
                       IppManager ipp_manager,
                       EsclManager escl_manager,
                       std::unique_ptr<mock_printer::MockPrinter> mock_printer)
    : usb_descriptors_(usb_descriptors),
      document_output_path_(document_output_path),
      http_output_dir_(http_output_dir),
      ipp_manager_(std::move(ipp_manager)),
      escl_manager_(std::move(escl_manager)),
      mock_printer_(std::move(mock_printer)),
      interface_managers_(usb_descriptors.interface_descriptors().size()) {}

bool UsbPrinter::IsIppUsb() const {
  int count = 0;
  const std::vector<UsbInterfaceDescriptor> interfaces =
      interface_descriptors();
  for (const auto& interface : interfaces) {
    if (interface.bInterfaceClass == 7 && interface.bInterfaceSubClass == 1 &&
        interface.bInterfaceProtocol == 4) {
      ++count;
    }
  }
  // An ipp-over-usb printer must have at least 2 ipp-over-usb interfaces.
  return count >= 2;
}

void UsbPrinter::HandleUsbRequest(int sockfd,
                                  const UsbipCmdSubmit& usb_request) {
  // Endpoint 0 is used for USB control requests.
  if (usb_request.header.ep == 0) {
    HandleUsbControl(sockfd, usb_request);
  } else {
    if (usb_request.header.direction == 1) {
      HandleBulkInRequest(sockfd, usb_request);
    } else {
      if (IsIppUsb()) {
        HandleIppUsbData(sockfd, usb_request);
      } else {
        HandleUsbData(sockfd, usb_request);
      }
    }
  }
}

void UsbPrinter::HandleUsbControl(int sockfd,
                                  const UsbipCmdSubmit& usb_request) const {
  UsbControlRequest control_request =
      CreateUsbControlRequest(usb_request.setup);
  int request_type = GetControlType(control_request.bmRequestType);
  switch (request_type) {
    case STANDARD_TYPE:
      HandleStandardControl(sockfd, usb_request, control_request);
      break;
    case CLASS_TYPE:
      HandlePrinterControl(sockfd, usb_request, control_request);
      break;
    case VENDOR_TYPE:
    case RESERVED_TYPE:
    default:
      LOG(ERROR) << "Unable to handle request of type: " << request_type;
      break;
  }
}

void UsbPrinter::HandleUsbData(int sockfd,
                               const UsbipCmdSubmit& usb_request) const {
  SmartBuffer data = ReceiveBuffer(sockfd, usb_request.transfer_buffer_length);
  size_t received = data.size();
  LOG(INFO) << "Received " << received << " bytes";
  // Acknowledge receipt of BULK transfer.
  SendUsbDataResponse(sockfd, usb_request, received);
  if (!document_output_path_.empty()) {
    LOG(INFO) << "Recording document...";
    AppendToFile(document_output_path_, data);
  }
}

void UsbPrinter::HandleIppUsbData(int sockfd,
                                  const UsbipCmdSubmit& usb_request) {
  SmartBuffer message =
      ReceiveBuffer(sockfd, usb_request.transfer_buffer_length);
  size_t received = message.size();
  LOG(INFO) << "Received " << received << " bytes";
  // Acknowledge receipt of BULK transfer.
  SendUsbDataResponse(sockfd, usb_request, received);

  HandleHttpData(usb_request, &message);
}

void UsbPrinter::HandleHttpData(const UsbipCmdSubmit& usb_request,
                                SmartBuffer* message) {
  InterfaceManager* im = GetInterfaceManager(usb_request.header.ep);

  if (!im->receiving_message()) {
    // If we're not currently receiving, |message| must be the start of a new
    // HTTP message. Parse the header and setup some fields to track state.
    std::optional<HttpRequest> opt_request = HttpRequest::Deserialize(message);
    if (!opt_request.has_value()) {
      LOG(ERROR) << "Incoming message is not valid HTTP; ignoring";
      return;
    }
    HttpRequest request = opt_request.value();
    im->set_receiving_message(true);
    im->set_request_header(request);
    im->set_receiving_chunked(request.IsChunkedMessage());
  }

  std::optional<SmartBuffer> payload =
      im->AddMessageAndReturnIfComplete(*message);
  if (!payload.has_value()) {
    return;
  }

  LogHttpHeaders(im->request_header());
  HttpResponse response =
      GenerateHttpResponse(im->request_header(), &payload.value());
  QueueHttpResponse(usb_request, response);
}

void UsbPrinter::HandleStandardControl(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  switch (control_request.bRequest) {
    case GET_STATUS:
      HandleGetStatus(sockfd, usb_request, control_request);
      break;
    case GET_DESCRIPTOR:
      HandleGetDescriptor(sockfd, usb_request, control_request);
      break;
    case GET_CONFIGURATION:
      HandleGetConfiguration(sockfd, usb_request, control_request);
      break;
    case CLEAR_FEATURE:
    case SET_FEATURE:
    case SET_ADDRESS:
    case SET_DESCRIPTOR:
    case SET_CONFIGURATION:
    case GET_INTERFACE:
    case SET_INTERFACE:
    case SET_FRAME:
      HandleUnsupportedRequest(sockfd, usb_request, control_request);
      break;
    default:
      LOG(ERROR) << "Received unknown control request "
                 << unsigned{control_request.bRequest};
      break;
  }
}

void UsbPrinter::HandlePrinterControl(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  switch (control_request.bRequest) {
    case GET_DEVICE_ID:
      HandleGetDeviceId(sockfd, usb_request, control_request);
      break;
    case GET_PORT_STATUS:
      break;
    case SOFT_RESET:
      break;
    default:
      LOG(ERROR) << "Unknown printer class request "
                 << unsigned{control_request.bRequest};
  }
}

InterfaceManager* UsbPrinter::GetInterfaceManager(int endpoint) {
  CHECK_GT(endpoint, 0) << "Received request on an invalid endpoint";
  // Since each interface contains a pair of in/out endpoints, we perform this
  // conversion in order to retrieve the corresponding interface number.
  // Examples:
  //   endpoints 1 and 2 both map to interface 0.
  //   endpoints 3 and 4 both map to interface 1.
  int index = (endpoint - 1) / 2;
  CHECK_LT(index, interface_managers_.size())
      << "Received request on an invalid endpoint";
  return &interface_managers_[index];
}

void UsbPrinter::LogHttpHeaders(const HttpRequest& request) {
  if (http_output_dir_.empty()) {
    return;
  }

  static int counter = 0;
  const std::string filename =
      base::StringPrintf("http-header-%04d.txt", ++counter);
  const base::FilePath filePath = http_output_dir_.Append(filename);
  base::File file(filePath,
                  base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open/create file at " << filePath;
    return;
  }

  const std::string msg =
      base::StringPrintf("%s\n", request.request_line.c_str());
  const int written = file.WriteAtCurrentPos(msg.c_str(), msg.length());
  if (written != msg.length()) {
    PLOG(ERROR) << "Failed to write HTTP request line to file at " << filePath;
    return;
  }

  const HttpHeaders& headers = request.headers;
  for (auto it = headers.begin(); it != headers.end(); ++it) {
    const std::string msg =
        base::StringPrintf("%s: %s\n", it->first.c_str(), it->second.c_str());
    const int written = file.WriteAtCurrentPos(msg.c_str(), msg.length());
    if (written != msg.length()) {
      PLOG(ERROR) << "Failed to write http headers to file at " << filePath;
      return;
    }
  }
}

HttpResponse UsbPrinter::GenerateHttpResponse(const HttpRequest& request,
                                              SmartBuffer* body) {
  LOG(INFO) << request.request_line;
  if (request.method == "POST" && request.uri == "/ipp/print") {
    if (!mock_printer_) {
      return GenerateHttpResponseFromIppManager(body);
    }

    HttpResponse retval = mock_printer_->GenerateHttpResponse(body);

    if (retval.default_response) {
      return GenerateHttpResponseFromIppManager(body);
    }

    return retval;

  } else if (base::StartsWith(request.uri, "/eSCL",
                              base::CompareCase::SENSITIVE)) {
    return escl_manager_.HandleEsclRequest(request, *body);
  }

  HttpResponse response;
  LOG(ERROR) << "Invalid method '" << request.method << "' and/or endpoint '"
             << request.uri << "'";
  response.status = "404 Not Found";
  return response;
}

HttpResponse UsbPrinter::GenerateHttpResponseFromIppManager(SmartBuffer* body) {
  HttpResponse response;
  std::optional<IppHeader> ipp_header = IppHeader::Deserialize(body);
  if (!ipp_header) {
    LOG(ERROR) << "Request does not contain a valid IPP header.";
    response.status = "415 Unsupported Media Type";
    return response;
  }
  if (!RemoveIppAttributes(body)) {
    LOG(ERROR) << "IPP request has malformed attributes section.";
    response.status = "415 Unsupported Media Type";
    return response;
  }
  response.status = "200 OK";
  response.headers["Content-Type"] = "application/ipp";
  response.body = ipp_manager_.HandleIppRequest(ipp_header.value(), *body);
  return response;
}

void UsbPrinter::HandleGetStatus(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetStatus " << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";
  uint16_t status = 0x1;  // Self-powered.
  SmartBuffer response(sizeof(status));
  response.Add(&status, sizeof(status));
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleGetDescriptor(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  switch (control_request.wValue1) {
    case USB_DESCRIPTOR_DEVICE:
      HandleGetDeviceDescriptor(sockfd, usb_request, control_request);
      break;
    case USB_DESCRIPTOR_CONFIGURATION:
      HandleGetConfigurationDescriptor(sockfd, usb_request, control_request);
      break;
    case USB_DESCRIPTOR_STRING:
      HandleGetStringDescriptor(sockfd, usb_request, control_request);
      break;
    case USB_DESCRIPTOR_INTERFACE:
    case USB_DESCRIPTOR_ENDPOINT:
    case USB_DESCRIPTOR_DEBUG:
      // Types that are known and not handled.
      HandleUnsupportedRequest(sockfd, usb_request, control_request);
      break;
    case USB_DESCRIPTOR_DEVICE_QUALIFIER:
      HandleGetDeviceQualifierDescriptor(sockfd, usb_request, control_request);
      break;
    default:
      LOG(ERROR) << "Unknown descriptor type request: "
                 << unsigned{control_request.wValue1};
      HandleUnsupportedRequest(sockfd, usb_request, control_request);
  }
}

void UsbPrinter::HandleGetDeviceDescriptor(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDeviceDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(sizeof(device_descriptor()));
  const UsbDeviceDescriptor& dev = device_descriptor();

  // If the requested number of bytes is smaller than the size of the device
  // descriptor then only send a portion of the descriptor.
  if (control_request.wLength < sizeof(dev)) {
    response.Add(&dev, control_request.wLength);
  } else {
    response.Add(dev);
  }

  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleGetConfigurationDescriptor(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetConfigurationDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(control_request.wLength);
  response.Add(configuration_descriptor());

  if (control_request.wLength == sizeof(configuration_descriptor())) {
    // Only the configuration descriptor itself has been requested.
    VLOG(1) << "Only configuration descriptor requested";
    SendUsbControlResponse(sockfd, usb_request, response.data(),
                           response.size());
    return;
  }

  const auto& interfaces = interface_descriptors();
  const auto& endpoints = endpoint_descriptors();

  // Place each interface and their corresponding endnpoint descriptors into the
  // response buffer.
  for (int i = 0; i < configuration_descriptor().bNumInterfaces; ++i) {
    const auto& interface = interfaces[i];
    response.Add(&interface, sizeof(interface));
    auto iter = endpoints.find(interface.bInterfaceNumber);
    if (iter == endpoints.end()) {
      LOG(ERROR) << "Unable to find endpoints for interface "
                 << unsigned{interface.bInterfaceNumber};
      exit(1);
    }
    for (const auto& endpoint : iter->second) {
      response.Add(&endpoint, sizeof(endpoint));
    }
  }

  CHECK_EQ(control_request.wLength, response.size())
      << "Response length does not match requested number of bytes";
  // After filling the buffer with all of the necessary descriptors we can send
  // a response.
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleGetDeviceQualifierDescriptor(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDeviceQualifierDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(sizeof(qualifier_descriptor()));
  response.Add(qualifier_descriptor());
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleGetStringDescriptor(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetStringDescriptor "
          << DescriptorTypeString(control_request.wValue1) << "["
          << unsigned{control_request.wValue0} << "]";

  int index = control_request.wValue0;
  const auto& strings = string_descriptors();
  CHECK(index < strings.size());
  SmartBuffer response(strings[index][0]);
  response.Add(strings[index].data(), strings[index][0]);
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleGetConfiguration(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetConfiguration " << unsigned{control_request.wValue1}
          << "[" << unsigned{control_request.wValue0} << "]";

  // Note: For now we only have one configuration set, so we just respond with
  // with |configuration_descriptor_.bConfigurationValue|.
  const auto& configuration = configuration_descriptor();
  SmartBuffer response(sizeof(configuration.bConfigurationValue));
  response.Add(&configuration.bConfigurationValue,
               sizeof(configuration.bConfigurationValue));
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleUnsupportedRequest(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleUnsupportedRequest "
          << StandardDeviceRequestString(control_request.bRequest) << ": "
          << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";
  SendUsbControlResponse(sockfd, usb_request, 0, 0);
}

void UsbPrinter::HandleGetDeviceId(
    int sockfd,
    const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  VLOG(1) << "HandleGetDeviceId " << unsigned{control_request.wValue1} << "["
          << unsigned{control_request.wValue0} << "]";

  SmartBuffer response(ieee_device_id().size());
  response.Add(ieee_device_id());
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::QueueHttpResponse(const UsbipCmdSubmit& usb_request,
                                   const HttpResponse& response) {
  SmartBuffer http_message;
  response.Serialize(&http_message);

  LOG(INFO) << "Queueing HTTP response...";
  InterfaceManager* im = GetInterfaceManager(usb_request.header.ep);
  im->QueueMessage(http_message);
}

void UsbPrinter::HandleBulkInRequest(int sockfd,
                                     const UsbipCmdSubmit& usb_request) {
  InterfaceManager* im = GetInterfaceManager(usb_request.header.ep);
  if (im->QueueEmpty()) {
    LOG(ERROR) << "No queued messages, sending empty response.";
    SendUsbControlResponse(sockfd, usb_request, 0, 0);
    return;
  }

  SmartBuffer http_message = im->PopMessage();

  size_t max_size = usb_request.transfer_buffer_length;

  UsbipRetSubmit response = CreateUsbipRetSubmit(usb_request);
  response.header.direction = 1;
  response.actual_length = std::min(max_size, http_message.size());
  LOG(INFO) << "Sending " << response.actual_length << " byte response.";

  SmartBuffer response_buffer = PackUsbipRetSubmit(response);
  if (http_message.size() > max_size) {
    size_t leftover_size = http_message.size() - max_size;
    SmartBuffer leftover(leftover_size);
    leftover.Add(http_message, max_size);
    http_message.Shrink(max_size);
    im->QueueMessage(leftover);
  }
  response_buffer.Add(http_message);
  SendBuffer(sockfd, response_buffer);
}
