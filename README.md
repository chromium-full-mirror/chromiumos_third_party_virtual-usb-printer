# Virtual USB Printer

Virtual USB Printer provides a server which can be used with USBIP in order to
emulate a USB printing device and bind it to the system as if it were physically
connected to a USB port.

Virtual USB Printer supports both regular USB printers as well as IPP-over-USB
devices.

## Motivation

This project was created in order to make on-device tests which check for any
regressions in the native USB printing system.

## Installation

As of https://crrev.com/c/3093381, `virtual-usb-printer` is built and
installed by default on all test images that support it.

If for some reason you need to build it yourself, you can `USE=usbip`
when building packages for your board - i.e.

```
USE="usbip" ./build_packages --board=$BOARD
```

*** note
`virtual-usb-printer` relies on `usbip` to manifest as a virtual USB device.
Most test images seem to come with this built-in by default. If you need
to build your own kernel with `usbip` support, make sure to build with
`CONFIG_USBIP_CORE` and `CONFIG_USBIP_VHCI_HCD`.
***

## How to Use

*** promo
`virtual-usb-printer` can behave like
*   a printer,
*   an eSCL scanner, or
*   a [scriptable mock printer](./mock_printer/README.md).
***

For ease of human operation, start `virtual-usb-printer` via its
Upstart unit. Pass the appropriate arguments to the invocation
as needed.

For example, to start and connect the `virtual-usb-printer` as an
IPP-over-USB printer, issue

```
start virtual-usb-printer USB_DESCRIPTORS=ippusb_printer.json IPP_ATTRIBUTES=ipp_attributes.json
```

Consult the
[Upstart config file](https://source.chromium.org/chromiumos/_/chromium/chromiumos/third_party/virtual-usb-printer/+/e7ce7b7c33c0f748617d321cba7fdc54b848a287:init/virtual-usb-printer.conf;l=19;drc=34ab3e4e2f18b354c998611ddec95cb38623865c)
to see the arguments understood in this context.

*** note
When managed by Upstart, `virtual-usb-printer` sends its output to the
system log.
***

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
+ `--output_log_dir` - directory path specifying where scan settings will be logged

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
