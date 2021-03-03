# Virtual USB Printer

Virtual USB Printer provides a server which can be used with USBIP in order to
emulate a USB printing device and bind it to the system as if it were physically
connected to a USB port.

Virtual USB Printer supports both regular USB printers as well as IPP-over-USB
devices.

## Motivation

This project was created in order to make on-device tests which check for any
regressions in the native USB printing system.

## Requirements

+ USBIP - userspace program for binding virtual device to system
+ USBIP kernel modules:
  + CONFIG\_USBIP\_CORE
  + CONFIG\_USBIP\_VHCI\_HCD

In order to actually "connect" the virtual printer to the system the USBIP
program is used.

USBIP is typically used to export real USB devices over a network so that other
systems on the network can bind them and treat them as connected devices.

For the purposes of the virtual printer, only the client portion of USBIP is
required so that the virtual device can be bound to the system.

## Installation

Currently the USBIP program is only installed by default for the elm and betty
boards.

In order to install it on your test image, you need to build an image with the
`usbip` USE flag enabled. To do this, all you need to to is run `build_packages`
with the USE flag enabled.

```
USE="usbip" ./build_packages --board=$BOARD
```

## How to Use

Before the first use you have to load a required kernel module:

```
modprobe vhci-hcd
```

Run the virtual-usb-printer program:

```
virtual-usb-printer --descriptors_path=<path>
```

(*Optional*) Check the list of exported printers with USBIP:

```
usbip list -r localhost
```

Should return something like this:

```
Exportable USB devices
======================
 - localhost
        1-1: Canon, Inc. : unknown product (04a9:27e8)
           : /sys/devices/pci0000:00/0000:00:01.2/usb1/1-1
           : (Defined at Interface level) (00/00/00)
           : 0 - Printer / Printer Bidirectional (07/01/02)
```

Bind the virtual printer to the system:

```
usbip attach -r localhost -b 1-1
```

Once the device has been attached it should appear in the list of connected USB
devices (`lsusb`), and can be used with the native printing system in Chrome OS.

## Configuration

The printer's USB descriptors and defined IPP attributes can be configured using
a JSON file and are loaded at run-time using command line flags. Example
configurations can be found in the `config/` directory.

The configuration files can be loaded with the following flags:

+ `--descriptors_path` - full path to the JSON file which defines the USB
  descriptors
+ `--attributes_path` - full path to the JSON file which defines the supported
  IPP attributes
  + Only needed for IPP-over-USB printer configurations
+ `--record_doc_path` - full path to the file used to record documents received
  from print jobs

## Using in Tast

Refer to these existing tast tests for examples of how to use the
`virtual-usb-printer` to test the Chromium OS printing stack.

### Print Tests

+ [printer.AddUSBPrinter](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/printer/add_usb_printer.go)
  + Tests that adding a basic USB printer works correctly
+ [printer.PrintUSB](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/printer/print_usb.go)
  + Tests that the full print pipeline works correctly for a basic USB printer
+ [printer.PrintIPPUSB](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/printer/print_ippusb.go)
  + Tests that the full print pipeline for IPP-over-USB printing works correctly
  + This also tests that the [automatic\_usb\_printer\_configurer](https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/chromeos/printing/automatic_usb_printer_configurer.h) is able to automatically configure an IPP Everywhere printer
+ [printer.IPPUSBPPDNoCopies](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/printer/ippusb_ppd_no_copies.go),
  [printer.IPPUSBPPDCopiesUnsupported](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/printer/ippusb_ppd_copies_unsupported.go),
  and
  [printer.IPPUSBPPDCopiesSupported](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/printer/ippusb_ppd_copies_supported.go)
  + Tests that the CUPS understands whether or not printers support
    print job copies (duplication).

### Scan Tests

+ [documentscanapi.Scan](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/documentscanapi/scan.go)
  + Tests the document scanner app.
+ [scanner.EnumerateIPPUSB](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/scanner/enumerate_ipp_usb.go)
  + Tests that
    [lorgnette](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/lorgnette/)
    can correctly enumerate IPP USB devices.
+ [scanner.ScanESCLIPP](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/scanner/scan_escl_ipp.go)
  + Tests basic scanning functionality via IPP USB.
