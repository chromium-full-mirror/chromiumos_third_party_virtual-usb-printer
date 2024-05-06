// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_device.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "virtual-usb-printer/common/smart_buffer.h"
#include "virtual-usb-printer/virtual_usb_device/device_descriptors.h"
#include "virtual-usb-printer/virtual_usb_device/load_config.h"
#include "virtual-usb-printer/virtual_usb_device/op_commands.h"
#include "virtual-usb-printer/virtual_usb_device/usb_util.h"

const char* descriptors_json_str = R"({
    "device_descriptor": {
        "bLength": 18,
        "bDescriptorType": 1,
        "bcdUSB": 272,
        "bDeviceClass": 0,
        "bDeviceSubClass": 0,
        "bDeviceProtocol": 0,
        "bMaxPacketSize0": 8,
        "idVendor": 6353,
        "idProduct": 20574,
        "bcdDevice": 0,
        "iManufacturer": 1,
        "iProduct": 2,
        "iSerialNumber": 1,
        "bNumConfigurations": 1
    },
    "configuration_descriptor": {
        "bLength": 9,
        "bDescriptorType": 2,
        "wTotalLength": 32,
        "bNumInterfaces": 1,
        "bConfigurationValue": 1,
        "iConfiguration": 0,
        "bmAttributes": 128,
        "bMaxPower": 0
    },
    "device_qualifier_descriptor": {
        "bLength": 10,
        "bDescriptorType": 6,
        "bcdUSB": 272,
        "bDeviceClass": 0,
        "bDeviceSubClass": 0,
        "bDeviceProtocol": 0,
        "bMaxPacketSize0": 255,
        "bNumConfigurations": 1,
        "bReserved": 0
    },
    "interface_descriptors": [{
  "bLength": 9,
  "bDescriptorType": 4,
  "bInterfaceNumber": 0,
  "bAlternateSetting": 0,
  "bNumEndpoints": 2,
        "bInterfaceClass": 7,
        "bInterfaceSubClass": 1,
        "bInterfaceProtocol": 2,
        "iInterface": 0,
        "endpoints": [{
    "bLength": 7,
          "bDescriptorType": 5,
          "bEndpointAddress": 1,
          "bmAttributes": 2,
          "wMaxPacketSize": 512,
          "bInterval": 0
        }, {
          "bLength": 7,
          "bDescriptorType": 5,
          "bEndpointAddress": 129,
          "bmAttributes": 2,
          "wMaxPacketSize": 512,
          "bInterval": 0
        }]
    }],
    "language_descriptor": {
        "bLength": 4,
        "bDescriptorType": 3,
        "langID1": 9,
        "langID2": 4
    },
    "string_descriptors": [
        "DavieV",
        "Virtual USB Printer"
    ],
    "ieee_device_id": {
        "bLength1": 0,
        "bLength2": 26,
        "message": "MFG:DV3;CMD:PDF;MDL:VTL;"
    }
})";

class UsbDeviceTest : public testing::Test {
 public:
  UsbDeviceTest() {
    std::optional<UsbDescriptors> descriptors =
        UsbDescriptors::CreateFromJson(descriptors_json_str);
    if (descriptors.has_value()) {
      usb_device_.reset(new UsbDevice(descriptors.value()));
    }
  }

 protected:
  std::unique_ptr<UsbDevice> usb_device_;
};

TEST_F(UsbDeviceTest, DeviceDescriptor) {
  ASSERT_NE(usb_device_, nullptr);
  UsbDeviceDescriptor descriptor = usb_device_->device_descriptor();
  EXPECT_EQ(descriptor.bLength, 18);
  EXPECT_EQ(descriptor.bDescriptorType, 1);
  EXPECT_EQ(descriptor.bcdUSB, 272);
  EXPECT_EQ(descriptor.bDeviceClass, 0);
  EXPECT_EQ(descriptor.idVendor, 6353);
  EXPECT_EQ(descriptor.idProduct, 20574);
  EXPECT_EQ(descriptor.iManufacturer, 1);
}

TEST_F(UsbDeviceTest, ConfigurationDescriptor) {
  ASSERT_NE(usb_device_, nullptr);
  UsbConfigurationDescriptor descriptor =
      usb_device_->configuration_descriptor();
  EXPECT_EQ(descriptor.bLength, 9);
  EXPECT_EQ(descriptor.bDescriptorType, 2);
  EXPECT_EQ(descriptor.wTotalLength, 32);
  EXPECT_EQ(descriptor.bNumInterfaces, 1);
  EXPECT_EQ(descriptor.bConfigurationValue, 1);
  EXPECT_EQ(descriptor.iConfiguration, 0);
  EXPECT_EQ(descriptor.bmAttributes, 128);
  EXPECT_EQ(descriptor.bMaxPower, 0);
}

TEST_F(UsbDeviceTest, QualifierDescriptor) {
  ASSERT_NE(usb_device_, nullptr);
  UsbDeviceQualifierDescriptor descriptor = usb_device_->qualifier_descriptor();
  EXPECT_EQ(descriptor.bLength, 10);
  EXPECT_EQ(descriptor.bDescriptorType, 6);
  EXPECT_EQ(descriptor.bcdUSB, 272);
  EXPECT_EQ(descriptor.bDeviceClass, 0);
  EXPECT_EQ(descriptor.bDeviceSubClass, 0);
  EXPECT_EQ(descriptor.bDeviceProtocol, 0);
  EXPECT_EQ(descriptor.bMaxPacketSize0, 255);
  EXPECT_EQ(descriptor.bNumConfigurations, 1);
  EXPECT_EQ(descriptor.bReserved, 0);
}

TEST_F(UsbDeviceTest, IeeeDeviceId) {
  ASSERT_NE(usb_device_, nullptr);
  std::vector<char> id = usb_device_->ieee_device_id();
  EXPECT_NE(id.size(), 0);
  std::string ieee_id(id.begin(), id.end());
  ieee_id.erase(remove_if(ieee_id.begin(), ieee_id.end(), isspace),
                ieee_id.end());
  bool found = ieee_id.find("MFG:DV3;CMD:PDF;MDL:VTL;") != std::string::npos;
  EXPECT_TRUE(found);
}

TEST_F(UsbDeviceTest, InterfaceDescriptor) {
  ASSERT_NE(usb_device_, nullptr);
  std::vector<UsbInterfaceDescriptor> interface_desc =
      usb_device_->interface_descriptors();
  ASSERT_GE(interface_desc.size(), 0);

  EXPECT_EQ(interface_desc[0].bLength, 9);
  EXPECT_EQ(interface_desc[0].bDescriptorType, 4);
  EXPECT_EQ(interface_desc[0].bInterfaceNumber, 0);
  EXPECT_EQ(interface_desc[0].bAlternateSetting, 0);
  EXPECT_EQ(interface_desc[0].bNumEndpoints, 2);
  EXPECT_EQ(interface_desc[0].bInterfaceClass, 7);
  EXPECT_EQ(interface_desc[0].bInterfaceSubClass, 1);
  EXPECT_EQ(interface_desc[0].bInterfaceProtocol, 2);
  EXPECT_EQ(interface_desc[0].iInterface, 0);
}

TEST_F(UsbDeviceTest, UsbRequest_HandleStandardControl) {
  ASSERT_NE(usb_device_, nullptr);
  Urb req;
  req.direction = 1;
  req.ep = 0;
  req.transfer_buffer_length = 0;
  {
    req.setup = 0x8006000100001200;  // USB_DESCRIPTOR_DEVICE request.

    std::optional<SmartBuffer> result =
        usb_device_->HandleUsbRequest(req, SmartBuffer());
    EXPECT_TRUE(result.has_value());
    ASSERT_GT(result.value().size(), 0);
    UsbDeviceDescriptor descriptor;
    memcpy(&descriptor, result.value().data(), sizeof(UsbDeviceDescriptor));

    EXPECT_EQ(descriptor.bLength, 18);
    EXPECT_EQ(descriptor.bDescriptorType, 1);
    EXPECT_EQ(descriptor.bcdUSB, 272);
    EXPECT_EQ(descriptor.bDeviceClass, 0);
    EXPECT_EQ(descriptor.bDeviceSubClass, 0);
    EXPECT_EQ(descriptor.bDeviceProtocol, 0);
    EXPECT_EQ(descriptor.bMaxPacketSize0, 8);
    EXPECT_EQ(descriptor.idVendor, 6353);
    EXPECT_EQ(descriptor.idProduct, 20574);
    EXPECT_EQ(descriptor.iManufacturer, 1);
    EXPECT_EQ(descriptor.iProduct, 2);
    EXPECT_EQ(descriptor.iSerialNumber, 1);
    EXPECT_EQ(descriptor.bNumConfigurations, 1);
  }

  {
    req.direction = 0;
    req.ep = 0;
    req.transfer_buffer_length = 0;
    req.setup = 0x8006000200000A00;  // USB_DESCRIPTOR_CONFIGURATION request.
    std::optional<SmartBuffer> result =
        usb_device_->HandleUsbRequest(req, SmartBuffer());
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(result.value().size(), 0);
    UsbConfigurationDescriptor descriptor;
    memcpy(&descriptor, result.value().data(),
           sizeof(UsbConfigurationDescriptor));

    EXPECT_EQ(descriptor.bLength, 9);
    EXPECT_EQ(descriptor.bDescriptorType, 2);
    EXPECT_EQ(descriptor.wTotalLength, 32);
    EXPECT_EQ(descriptor.bNumInterfaces, 1);
    EXPECT_EQ(descriptor.bConfigurationValue, 1);
    EXPECT_EQ(descriptor.iConfiguration, 0);
    EXPECT_EQ(descriptor.bmAttributes, 128);
    EXPECT_EQ(descriptor.bMaxPower, 0);
  }

  {
    req.direction = 1;
    req.ep = 0;
    req.transfer_flags = 512;
    req.transfer_buffer_length = 255;
    req.start_frame = 0;
    req.num_of_packets = 0;
    req.setup = 0x800601030000FF00;  // USB_DESCRIPTOR_STRING
    std::optional<SmartBuffer> result =
        usb_device_->HandleUsbRequest(req, SmartBuffer());
    EXPECT_TRUE(result.has_value());
    SmartBuffer data = result.value();
    std::string string_descriptor(data.contents().begin(),
                                  data.contents().end());

    auto chars = ConvertStringToStringDescriptor("DavieV");
    std::string expected_string(chars.begin(), chars.end());
    bool exists =
        (string_descriptor.find(expected_string) != std::string::npos);
    EXPECT_TRUE(exists);
  }

  {
    req.direction = 0;
    req.ep = 0;
    req.transfer_flags = 0;
    req.transfer_buffer_length = 0;
    req.start_frame = 0;
    req.num_of_packets = 0;
    req.interval = 0;

    req.setup = 0x8000000000000200;  // GET_STATUS
    std::optional<SmartBuffer> result =
        usb_device_->HandleUsbRequest(req, SmartBuffer());
    EXPECT_TRUE(result.has_value());
    uint16_t status;
    memcpy(&status, result.value().data(), sizeof(uint16_t));
    EXPECT_EQ(status, 1);  // response is 0x1.
  }

  {
    req.setup = 0x0101000000000000;  // CLEAR_FEATURE
    std::optional<SmartBuffer> result =
        usb_device_->HandleUsbRequest(req, SmartBuffer());
    EXPECT_TRUE(result.has_value());
    // Empty response buffer is success since these
    // request are unsupported for now.
    EXPECT_EQ(result.value().size(), 0);
  }

  {
    req.setup = 0x0005000000000000;  // SET_ADDRESS
    std::optional<SmartBuffer> result =
        usb_device_->HandleUsbRequest(req, SmartBuffer());
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 0);  // Empty response buffer is success.
  }

  {
    req.setup = 0x0007000000000000;  //  SET_DESCRIPTOR
    std::optional<SmartBuffer> result =
        usb_device_->HandleUsbRequest(req, SmartBuffer());
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 0);  // Empty response buffer is success.
  }

  {
    req.setup = 0x0009010000000000;  //  SET_CONFIGURATION
    std::optional<SmartBuffer> result =
        usb_device_->HandleUsbRequest(req, SmartBuffer());
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 0);  // Empty response buffer is success.
  }

  {
    req.setup = 0x810A000000000000;  //  GET_INTERFACE
    std::optional<SmartBuffer> result =
        usb_device_->HandleUsbRequest(req, SmartBuffer());
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 0);  // Empty response buffer is success.
  }

  {
    req.setup = 0x010B000001000000;  //  SET_INTERFACE
    std::optional<SmartBuffer> result =
        usb_device_->HandleUsbRequest(req, SmartBuffer());
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 0);  // Empty response buffer is success.
  }
}

TEST_F(UsbDeviceTest, UsbRequest_HandleDeviceControl) {
  Urb req;
  req.direction = 1;
  req.ep = 0;
  req.transfer_flags = 512;
  req.transfer_buffer_length = 255;
  req.start_frame = 0;
  req.num_of_packets = 0;
  req.interval = 0;

  {
    req.setup = 0xA10000000000FF00;  // GET_DEVICE_ID request
    std::optional<SmartBuffer> result =
        usb_device_->HandleUsbRequest(req, SmartBuffer());
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(result.value().size(), 0);
  }
}

TEST_F(UsbDeviceTest, UsbRequest_HandleBulkInRequest) {
  ASSERT_NE(usb_device_, nullptr);
  Urb req;
  req.direction = 1;
  req.ep = 2;
  req.transfer_flags = 512;
  req.transfer_buffer_length = 8192;
  req.start_frame = 0;
  req.num_of_packets = 0;
  req.interval = 3278;
  req.setup = 0x0;
  std::optional<SmartBuffer> result =
      usb_device_->HandleUsbRequest(req, SmartBuffer());
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value().size(), 0);
}
