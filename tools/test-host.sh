#!/usr/bin/env bash
# Compiles and runs the firmware's host tests (tests/host): firmware logic
# that doesn't need the board, built with the computer's C++ compiler and
# small stand-ins for the ESP32 APIs it uses.
set -euo pipefail
cd "$(dirname "$0")/.."

CXX=${CXX:-c++}
OUT=build/host-tests
mkdir -p "$OUT"
status=0
for test in tests/host/*_test.cpp; do
  name=$(basename "$test" .cpp)
  echo "### $name"
  "$CXX" -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Werror \
    -I tests/host/stubs -o "$OUT/$name" "$test"
  "$OUT/$name" || status=1
done
exit $status
