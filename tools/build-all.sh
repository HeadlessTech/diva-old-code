#!/usr/bin/env bash
# Builds the firmware for a set of real machine configurations and checks that
# config.h mistakes stop the build with the right message. Run after every
# change, before testing on a board:
#
#   tools/build-all.sh            all builds and checks
#   tools/build-all.sh --quick    one build per machine type, all checks
#
# Each build uses a copy of All_in_One with its own config.h, kept in build/
# (ignored by git) so the next run only rebuilds what changed. The real
# config.h is never touched.
set -uo pipefail
cd "$(dirname "$0")/.."

FQBN=esp32:esp32:esp32
QUICK=0; [ "${1:-}" = "--quick" ] && QUICK=1
WORK=build/build-all
mkdir -p "$WORK"

# config ID TYPE SIZE NETWORK SSID PASSWORD RFID BKASH CASH SERVER
config() {
  cat <<EOF
#define MACHINE_ID "$1"
#define MACHINE_TYPE $2
#define MACHINE_SIZE $3
#define NETWORK $4
#define WIFI_SSID "$5"
#define WIFI_PASSWORD "$6"
#define HAS_RFID $7
#define HAS_BKASH $8
#define HAS_CASH $9
#define SERVER ${10}
EOF
}

prepare() {  # name, config text -> sketch folder path
  local dir="$WORK/$1/All_in_One"
  mkdir -p "$dir"
  # keep timestamps so unchanged files aren't rebuilt
  rsync -a --delete --exclude config.h All_in_One/ "$dir/"
  printf '%s\n' "$2" > "$dir/config.h.new"
  if cmp -s "$dir/config.h.new" "$dir/config.h"; then rm "$dir/config.h.new"; else mv "$dir/config.h.new" "$dir/config.h"; fi
  echo "$dir"
}

pass=0 fail=0
ok()  { echo "  PASS  $*"; pass=$((pass + 1)); }
bad() { echo "  FAIL  $*"; fail=$((fail + 1)); }

build() {  # name, config text
  local name=$1 dir out
  dir=$(prepare "$name" "$2")
  out=$(arduino-cli compile --fqbn "$FQBN" --build-path "$WORK/$name/build" "$dir" 2>&1)
  if [ $? -eq 0 ]; then
    local flash ram
    flash=$(echo "$out" | sed -nE 's/^Sketch uses ([0-9]+) bytes.*/\1/p')
    ram=$(echo "$out" | sed -nE 's/^Global variables use ([0-9]+) bytes.*/\1/p')
    ok "$(printf '%-22s flash %7s  static RAM %6s' "$name" "$flash" "$ram")"
  else
    bad "$name does not build:"
    echo "$out" | grep -E "error|Error" | head -5 | sed 's/^/        /'
  fi
}

rejects() {  # name, expected message, config text
  local name=$1 expect=$2 dir out
  dir=$(prepare "reject-$name" "$3")
  out=$(arduino-cli compile --fqbn "$FQBN" --build-path "$WORK/reject-$name/build" "$dir" 2>&1)
  if [ $? -ne 0 ] && echo "$out" | grep -qF -- "$expect"; then
    ok "rejects $name"
  else
    bad "should reject $name with \"$expect\""
    echo "$out" | grep -E "error" | head -3 | sed 's/^/        /'
  fi
}

echo "== machine configurations"
build standard-160-gsm "$(config JJMBRM160 TYPE_STANDARD 160 NETWORK_GSM "" "" 1 0 0 SERVER_PRODUCTION)"
build two-group-160-gsm "$(config BBKD01 TYPE_TWO_GROUP_160 160 NETWORK_GSM "" "" 1 1 0 SERVER_PRODUCTION)"
if [ $QUICK = 0 ]; then
  build standard-20-all-modules "$(config BAIO20 TYPE_STANDARD 20 NETWORK_GSM "" "" 1 1 1 SERVER_PRODUCTION)"
  build standard-60-gsm "$(config bracit-brm60-01 TYPE_STANDARD 60 NETWORK_GSM "" "" 1 1 0 SERVER_PRODUCTION)"
  build standard-40-cash-only "$(config BBCM40 TYPE_STANDARD 40 NETWORK_GSM "" "" 0 0 1 SERVER_PRODUCTION)"
  build standard-120-staging "$(config CAAB120 TYPE_STANDARD 120 NETWORK_GSM "" "" 1 1 1 SERVER_STAGING)"
  build standard-160-wifi "$(config UCEP02 TYPE_STANDARD 160 NETWORK_WIFI "Diva" "password" 1 1 0 SERVER_PRODUCTION)"
fi

echo "== config.h mistakes stop the build"
base() { config JJMBRM160 TYPE_STANDARD 160 NETWORK_GSM "" "" 1 0 0 SERVER_PRODUCTION; }
rejects machine-id-not-set "set MACHINE_ID" "$(base | sed 's/^#define MACHINE_ID .*/#define MACHINE_ID "CHANGE_ME"/')"
rejects machine-id-with-space "may only contain letters" "$(base | sed 's/^#define MACHINE_ID .*/#define MACHINE_ID "JJM 160"/')"
rejects missing-setting "HAS_RFID, HAS_BKASH and HAS_CASH must all be there" "$(base | grep -v HAS_CASH)"
rejects misspelled-setting "HAS_RFID, HAS_BKASH and HAS_CASH must all be there" "$(base | sed 's/HAS_RFID 1/HAS_RFIDX 1/')"
rejects misspelled-type "MACHINE_TYPE must be" "$(base | sed 's/TYPE_STANDARD/TYPE_STANDRD/')"
rejects bad-size "MACHINE_SIZE must be" "$(base | sed 's/MACHINE_SIZE 160/MACHINE_SIZE 100/')"
rejects module-not-0-or-1 "must each be 1 (installed) or 0" "$(base | sed 's/HAS_RFID 1/HAS_RFID 2/')"
rejects no-payment-module "no payment module" "$(base | sed 's/HAS_RFID 1/HAS_RFID 0/')"
rejects wifi-without-name "WIFI_SSID is empty" "$(base | sed 's/NETWORK_GSM/NETWORK_WIFI/')"
rejects old-style-switch "Old setting found" "$({ base; echo "#define USE_WIFI"; })"
rejects two-group-with-cash "can't have HAS_CASH 1" "$(config BBKD01 TYPE_TWO_GROUP_160 160 NETWORK_GSM "" "" 1 1 1 SERVER_PRODUCTION)"
rejects two-group-not-160 "needs MACHINE_SIZE 160" "$(config BBKD01 TYPE_TWO_GROUP_160 120 NETWORK_GSM "" "" 1 1 0 SERVER_PRODUCTION)"
rejects two-group-without-rfid "needs HAS_RFID 1" "$(config BBKD01 TYPE_TWO_GROUP_160 160 NETWORK_GSM "" "" 0 1 0 SERVER_PRODUCTION)"

echo
echo "passed: $pass  failed: $fail"
[ $fail -eq 0 ]
