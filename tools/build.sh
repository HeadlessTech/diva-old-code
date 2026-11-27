#!/usr/bin/env bash
# Builds the All_in_One firmware with arduino-cli, as configured in
# All_in_One/config.h, and optionally flashes it and shows the serial log.
#
#   tools/build.sh                     build only
#   tools/build.sh -p /dev/cu.usbserial-0001          build and flash
#   tools/build.sh -p /dev/cu.usbserial-0001 -m       build, flash, show the log
#
# Windows: use flash.bat in the repository root instead.
set -euo pipefail
cd "$(dirname "$0")/.."

FQBN=esp32:esp32:esp32           # "ESP32 Dev Module", default options
CORE_VERSION=2.0.17
INDEX_URL=https://espressif.github.io/arduino-esp32/package_esp32_index.json

port="" monitor=0 build_path=""
while [ $# -gt 0 ]; do
  case "$1" in
    -p) port="$2"; shift 2 ;;
    -m) monitor=1; shift ;;
    --build-path) build_path="$2"; shift 2 ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done

command -v arduino-cli >/dev/null || { echo "arduino-cli not found: https://arduino.github.io/arduino-cli/latest/installation/" >&2; exit 1; }

# The firmware is built and tested with exactly this ESP32 board package.
if ! arduino-cli core list --format json | grep -q "\"installed_version\": *\"$CORE_VERSION\""; then
  echo "Installing ESP32 board package $CORE_VERSION ..."
  arduino-cli core update-index --additional-urls "$INDEX_URL"
  arduino-cli core install "esp32:esp32@$CORE_VERSION" --additional-urls "$INDEX_URL"
fi

args=(compile --fqbn "$FQBN" --warnings default)
[ -n "$build_path" ] && args+=(--build-path "$build_path")
[ -n "$port" ] && args+=(--upload --port "$port")
arduino-cli "${args[@]}" All_in_One

if [ -n "$port" ] && [ "$monitor" = 1 ]; then
  echo "Showing the machine's log (Ctrl+C to stop) ..."
  arduino-cli monitor --port "$port" --config baudrate=115200
fi
