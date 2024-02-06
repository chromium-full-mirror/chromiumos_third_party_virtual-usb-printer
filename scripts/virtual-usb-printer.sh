#!/bin/bash
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script setups virtual usb printer. After setup, usb devices can be
# listed, attached and accessed.This script launches host and a device binary
# where the device binary could be of multiple type e.g scanner, printer.
# The device binary is selected based on command line args passed, which is
# specific to the device.

DEFAULT_HOST_PORT=3240
DEFAULT_DEVICE_PORT=3241

# Starts usbip host process with given port number as argument and wait
# until the host process is ready to process usbip messages.
function start_host() {
  while IFS= read -r line
  do
    echo "${line}"
    if [[ "${line}" = "USBIPHost::READY" ]];
      then return 0;
    else
      sleep 1
    fi
  done < <(stdbuf -oL /usr/local/bin/virtual-usbip-host "$@")
  return 1;
}

# Starts printer device process with given port number on which device listens
# and also usbip host port number to which this devices registers to.
function setup_device() {
  cmd=$1; shift
  while IFS= read -r line;
  do
    echo "${line}"
    if [[ "${line}" = "USBDevice::READY" ]];
    then return 0;
    else
      echo "${line}"
      sleep 1
    fi
  done < <(stdbuf -oL "${cmd}" "$@")
  return 1;
}

descriptors_path=""
is_ipp_printer=""
host_port=${DEFAULT_HOST_PORT}
device_port=${DEFAULT_DEVICE_PORT}


usage() {
  printf "Usage: \n"
    " $0  --descriptors_path=<path>\n"
    "    [--attributes_path=<path>]\n"
    "    [--scanner_capabilities_path=<path>]\n"
    "    [--mock_printer_script=<path>]\n"
    "    [--record_doc_path=<path>]\n"
    "    [--http_header_output_dir=<path>]\n"
  echo "Setup the virtual usb printer"
  exit 1
}

function main() {
  if [ "$#" -lt 1 ]; then
    usage
  fi

  commonargs=()

  while [ "$1" != "" ]; do
    key=${1%=*}
    value=${1#*=}
    case ${key} in
        --descriptors_path )
            descriptors_path=${value}
        ;;
        --attributes_path )
            ipp_attributes_path=${value}
            is_ipp_printer=true
        ;;
        --port )
            device_port=${value}
        ;;
        --host_port )
            host_port=${value}
        ;;
        -h | --help )    usage
        ;;
        * )
          commonargs+=("$1")
        ;;
    esac
    shift
  done

  if [ ! "${host_port}" -ge 0 ]; then
    host_port=${DEFAULT_HOST_PORT}
  fi

  if [ ! "${device_port}" -ge 0 ]; then
    device_port=${DEFAULT_DEVICE_PORT}
  fi

  if [ -z "${descriptors_path}" ]; then
    echo "start_printer: Missing --descriptors_path."
    exit 1;
  fi

  echo "Starting virtual usbip host....."
  if ! start_host --port="${host_port}";then
    echo "Host launch failed"
    exit 1
  fi

  echo "${descriptors_path}"
  echo "${ipp_attributes_path}"
  echo "${host_port}"
  echo "${device_port}"
  echo "${commonargs[@]}"
  #Starting virtual usb device as per command line args
  device_bin=
  if [[ -n ${is_ipp_printer} ]]; then
    # launch multi functionality usb printer
    echo "Starting virtual-printer..."
    device_bin=/usr/local/bin/virtual-printer
    setup_device "${device_bin}" \
        --descriptors_path="${descriptors_path}" \
        --attributes_path="${ipp_attributes_path}" \
        --host_port="${host_port}" \
        --port="${device_port}" \
        "${commonargs[@]}"
  else
    # launch raw usb printer
    echo "Starting virtual-rawpriner..."
    device_bin=/usr/local/bin/virtual-rawprinter
    setup_device "${device_bin}" \
        --descriptors_path="${descriptors_path}" \
        --host_port="${host_port}" \
        --port="${device_port}" \
        "${commonargs[@]}"
  fi

  # This message tells external entity(e.g tast tests) that virtual-usb-printer
  # setup is complete and ready to process any usbip messages.
  echo "virtual-usb-printer: ready to accept connections"
  wait
}

echo "Starting virtual usb printer VERSION:NEW"
main "$@"
