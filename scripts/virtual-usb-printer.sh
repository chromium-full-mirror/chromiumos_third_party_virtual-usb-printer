#!/bin/bash
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script setups virtual usb printer. After setup, usb devices can be
# listed, attached and accessed.

DEFAULT_HOST_PORT=3240
DEFAULT_DEVICE_PORT=3241

# Starts usbip host process with given port number as argument and wait
# until the host process is ready to process usbip messages.
function start_host() {
  HOST_PORT=${1##*=}
  if [ ! "${HOST_PORT}" -ge 0 ]; then
    echo "start_host: Missing valid port."
    exit 1;
  fi

  while IFS= read -r line;
  do
    if [[ "${line}" = "USBIPHost::READY" ]];
    then break;
    else
      sleep 1
    fi
  done < <(stdbuf -oL /usr/local/bin/virtual-usbip-host --port="${HOST_PORT}")
}

# Starts printer device process with given port number on which device listens
# and also usbip host port number to which this devices registers to.
function start_printer() {
  HOST_PORT=${1##*=}; shift
  DEVICE_PORT=${1##*=}; shift
  DESCRIPTORS_PATH=${1##*=}; shift

  echo "host port: ${HOST_PORT}"
  echo "device port: ${DEVICE_PORT}"
  echo "descriptors_path: ${DESCRIPTORS_PATH}"

  if [ ! "${HOST_PORT}" -ge 0 ] || [ ! "${DEVICE_PORT}" -ge 0 ]; then
    echo "start_printer: Missing valid port or host port."
    exit 1;
  fi

  if [ -z "${DESCRIPTORS_PATH}" ]; then
    echo "start_printer: Missing --descriptors_path."
    exit 1;
  fi

  while IFS= read -r line;
  do
    if [[ "${line}" = "USBDevice::READY" ]];
    then break;
    else
      sleep 1
    fi
  done < <(stdbuf -oL /usr/local/bin/virtual-printer \
    --descriptors_path="${DESCRIPTORS_PATH}" "$@" \
    --host_port="${HOST_PORT}" \
    --port="${DEVICE_PORT}")
}

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

  echo "Starting Host....."
  start_host --port="${DEFAULT_HOST_PORT}"

  echo "Starting printer....."
  start_printer --host_port="${DEFAULT_HOST_PORT}" \
    --port="${DEFAULT_DEVICE_PORT}" \
    "$@"

  # This message tells external entity(e.g tast tests) that virtual-usb-printer
  # setup is complete and ready to process any usbip messages.
  echo "virtual-usb-printer: ready to accept connections"
  wait
}

echo "Starting virtual usb printer VERSION:NEW"
main "$@"
