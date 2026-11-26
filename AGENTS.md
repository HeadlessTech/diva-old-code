# AGENTS.md

Guide for developers and AI agents working on this repository. `README.md`
is the production team's flashing guide. This file covers how the firmware
works and how to change it safely.

## Context

Diva sanitary pad vending machines run on an ESP32 (ESP32 Dev Module) with a
composite-video screen, motors with limit switches, and optional RFID card
reader, bKash QR payment (server side) and cash note acceptor. They talk to
the **VMS server** (repo `HeadlessTech/vms`, see its `AGENTS.md`) over
**MQTT with TLS** through our Mosquitto broker **`iot-mqtt.headless.ltd:8883`**
(repo `HeadlessTech/iot-mqtt`), over a SIM800 GSM modem or Wi-Fi.

**`All_in_One`** is the one firmware for every live machine: standard sizes
20/40/60/120/160 and the two-group 160 (`MACHINE_TYPE TYPE_TWO_GROUP_160`).
`All_in_One_Weird` is deprecated (merged in). The other folders are older
cash/offline firmwares that upload over HTTP; leave them alone unless asked.

Machines handle money and people's access to products: keep changes small,
build every configuration, and test on a board before anything is flashed to
a machine. Never change the message formats without the matching VMS change.

## Layout (`All_in_One/`)

```
All_in_One.ino        setup(), loop() -> loopStandard() or loopTwoGroup(), buttons, onMqttMessage()
config.h              per-machine settings; the ONLY file production edits
config_check.h        rejects bad config.h at build time; derives USE_WIFI, IS_DEVELOPMENT, HAS_*_MODULE, IS_TWO_GROUP
broker.h              broker address and the machines' shared login ("vms"); password at the top
version.h             FIRMWARE_VERSION; FW_VERSION is sent in the "online" status
IoT_Root_CA.h         BearSSL trust anchors: our main + backup CA (generated in iot-mqtt)
Client.h              pickBestClient(): network up, then initializeSSL(); transport watchdog
GsmClient.h           SIM800 via TinyGSM on Serial2 (APN "INTERNET")
WifiClient.h          Wi-Fi station (WIFI_SSID / WIFI_PASSWORD from config.h)
SSL.h                 SSLClient (BearSSL) over the GSM or Wi-Fi client
MQTT.h                PubSubClient: connect (LWT, cleanSession=false), status, publish, recovery/reboot
RequestsResponse.h    sync and dispense requests; handleDispenseResponse(), handleSyncResponse()
Dispense.h            motor and limit-switch wiring per machine type; runs the motors
FeatureCash.h         cash note acceptor (pulse counting)
Global.h              state machine, pending request, NVS-saved history of dispense numbers, two-group state
display.h             screen: layout, displayInit(), showImage(), idle screen
display_*.h/.cpp      composite video driver (DAC on GPIO 25) and graphics, adapted
i_*.h                 168x124 screen images (and the 220x47 logo), 8-bit pixels in flash
src/                  the libraries, copied in by tools/vendor-libraries.sh (don't edit by hand)
```

`tools/`: `build-all.sh` (build every configuration + config checks),
`build.sh` (build/flash/monitor, macOS/Linux), `vendor-libraries.sh`
(library upgrades). `flash.bat`: production flashing on Windows.

## Building and testing

```bash
tools/build-all.sh            # before every commit: all machine types + 13 config.h mistakes
tools/build-all.sh --quick    # one build per machine type + the checks
tools/build.sh -p /dev/cu.usbserial-XXXX -m   # flash a test board and watch the log
```

- ESP32 board package **2.0.17** exactly (`config_check.h` enforces it);
  board `esp32:esp32:esp32` with default options.
- Libraries are in `All_in_One/src` (ArduinoJson 7.4.3, MultiButton 1.3.0,
  PubSubClient 2.8, Rdm6300 2.0.0, SSLClient 1.6.11, TinyGSM 0.12.0). Includes
  use `"src/<Lib>/..."`. To upgrade, run `tools/vendor-libraries.sh`, which
  also re-applies our one patch (below) and fails if it no longer fits.
- There are no automated tests on the host. Verification is: every
  configuration builds, then a board test with serial output against the
  staging server (`SERVER_STAGING`, `vms-new.headless.ltd`, `/dev` topics).
  Say in your summary what was built and what still needs the board.

## Configuration (config.h -> config_check.h)

Production sets values in `config.h`; `config_check.h` refuses missing,
misspelled or conflicting settings with a message naming the setting, then
derives the switches the code uses (`USE_WIFI`, `IS_DEVELOPMENT` = staging,
`HAS_RFID_MODULE`, `HAS_BKASH_MODULE`, `HAS_CASH_MODULE`, `IS_TWO_GROUP`).
Never set the derived switches by hand (it's rejected). When you add a
setting: add it to `config.h` with a comment, check it in `config_check.h`,
print it in the boot log, add a case to `tools/build-all.sh`, and document it
in `README.md`.

## Server contract (don't change without VMS)

Topics, with `/dev` appended on the staging server; client ID = machine ID,
login `vms` (the broker only allows a machine its own topics):

| Topic | Direction | Messages |
|---|---|---|
| `vms/<id>/pub` | machine -> server | `{"info":"sync"}`, `{"info":"sync","dispenseNo":N}` (confirms a dispense), `{"info":"dispense","rfid":"<card>"}` (no `rfid` = cash), plus `"size":"BELT"/"PANTY"` on two-group |
| `vms/<id>/sub` | server -> machine | dispense: `{"info":"dispense","status":200,"row","column" or "columns":[..],"dispenseNo","type":"bkash"?}` or an error status; sync: `{"info":"sync","status":200,"count":N,"stock":{sizeId:n}?}` |
| `vms/<id>/status` | machine -> server | `{"message":"online","fw":"2.0.0 STD ..."}` every 30 s; last will `{"message":"offline"}` |
| `vms/<id>/ping` | both | server sends `ping` (bKash checkout), machine answers `pong` |

Dispense statuses: 200 dispense, 400 unauthorized, 402 card blocked,
403 limit crossed, 404 no stock or size, 405 not enough points.

## How it works

- **Boot:** config banner, display, saved dispense history (NVS), network,
  TLS, MQTT connect (`cleanSession=false`, LWT), subscribe `sub` + `ping`,
  sync, idle screen.
- **Loop:** status every 30 s, sync every 90 s (and every 30 s while out of
  stock), pong when pinged, buttons, card reader, timeouts. Restart after 42
  days of uptime when idle.
- **Card/cash dispense:** the machine publishes a request and waits (25 s,
  then "try again"; a late reply still dispenses). The server answers 200 with
  a `dispenseNo` and resends the command until confirmed.
  `handleDispenseResponse()` saves the number first (NVS ring of 100), runs
  the motors, then confirms with `{"info":"sync","dispenseNo":N}`. A number
  already in the history is ignored, so redeliveries never dispense twice.
- **bKash:** the server sends the dispense command (`"type":"bkash"`) after
  payment; same handling.
- **Two-group:** a button chooses BELT or PANTY (state `AWAITING_RFID`, 15 s),
  then the card; the request carries `size`. Per-group stock comes from the
  sync reply.
- **Recovery:** most failures (publish failed, transport lost, preflight
  timeout) reboot the ESP32 on purpose: broken TLS sessions in SSLClient
  could not be recovered in place. After 4 failed MQTT attempts it reboots too.
- **Known fragility:** the dispense runs inside PubSubClient's message
  callback, and the motor loop calls `mqtt.loop()` again from there. It works
  with PubSubClient 2.8 (the dispense-number history prevents double
  dispensing) but don't build more on it.

## Memory (the main constraint)

Heap is about 300 KB at boot. Budget, roughly: display ~60 KB (one buffer
holding only the drawn rows: logo + one 168x124 image; the rest of the screen
shares one black row), Wi-Fi stack ~55 KB on Wi-Fi machines, SSLClient 8.6 KB
(BearSSL with a 2 KB record buffer). `initializeSSL()` reboots below 55 KB
free. The boot log prints free heap before/after the display and TLS.

- All screen images must be exactly 168x124 (checked at build time); they're
  drawn in the image area only when the picture changes. Drawing anywhere
  else lands in an invisible scratch row (see `CompositeGraphics::init`).
- Flash: the Wi-Fi build is at about 92% of the 1.25 MB app partition. The
  images are uncompressed (21 KB each); compress them before adding features
  that need more flash (e.g. remote updates).
- Avoid heap churn: fixed buffers for messages, no `String` building in hot
  paths, `printf` lines under 64 characters (longer ones allocate).

## TLS

- BearSSL via SSLClient over the GSM or Wi-Fi client. Its 2 KB buffer works
  because the broker honours the TLS "max fragment length" extension
  (checked: 512 and 2048 accepted).
- Trust anchors: our main and backup CA (valid 2020-2050). The broker
  certificate is valid 2020-2045. BearSSL checks certificate dates against
  the **build date** (no clock on the board): any build from 2020 on works
  until 2045. When the broker certificate is renewed, keep a start date in
  the past (iot-mqtt `scripts/make-ca.sh` does).
- **Our patch to SSLClient** (`tools/vendor-libraries.sh`): the TLS random
  generator is seeded from the ESP32 hardware RNG. Upstream reads an analog
  pin; the firmware used to pass A5 = GPIO 33, the column-6 motor pin.
  Never pass a pin that's in use.

## Pins

| GPIO | Standard | Two-group 160 |
|---|---|---|
| 23, 22, 4, 2, 32, 33, 26, 27 | motors, columns 1-8 | motors: col 1 = 23+22, col 2 = 4+2, cols 3-6 = 32, 33, 26, 27 |
| 21, 19, 18, 13 | limit switches (20-60: one per column; 120/160: shared by column pairs) | limit switches of cols 1 and 2 (two each) |
| 34, 35 | 35 = cash acceptor pulses (if cash) | limit switches cols 3-4 (34) and 5-6 (35); no internal pull-ups, the board needs resistors |
| 14 | cash acceptor enable | - |
| 39, 36 | cash button, reset button (sync; long press reconnect) | top (BELT) and bottom (PANTY) buttons |
| 5 | RFID reader (Serial1 RX) | same |
| 16, 17 | SIM800 modem (Serial2) | same |
| 25 | composite video (DAC) | same |

## Gotchas

- Machine ID = MQTT client ID = topic part. It must match the VMS dashboard.
- Two machines with the same ID knock each other off the broker.
- Two-group stock in VMS must have columns 1-2 as `BELT` and 3-6 as `PANTY`,
  matching the wiring, or a PANTY order runs a BELT motor.
- `broker.h` holds the shared machine password (committed on purpose: every
  machine carries it in flash anyway). Changing it means reflashing every
  machine.
- The code uses ArduinoJson 6-style calls (`StaticJsonDocument`,
  `containsKey`) on ArduinoJson 7: deprecation warnings are expected.
- `flash.bat` must keep Windows line endings (`.gitattributes`).

## Open questions (hardware team)

- Motor timeout: when a limit switch doesn't trigger in 8-10 s the motor is
  stopped but the dispense is still confirmed to the server (points and stock
  deducted, bKash not refunded). Is a timeout a jam (no product) or a switch
  fault (product out)? Reporting it needs a VMS change.
- Cash: pulses per ten-taka note (the code dispenses at 25 pulses).
- Two-group board: pull-up resistors on GPIO 34 and 35.
