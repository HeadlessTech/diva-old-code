# Diva vending machine firmware

**`All_in_One`** is the firmware for every Diva machine that talks to the VMS
server live (card, bKash, cash):

- standard machines of size 20, 40, 60, 120 and 160
- the two-group 160 machine (BELT on the top button, PANTY on the bottom)

The other folders are older firmwares for cash/offline machines that upload
over HTTP; they are not part of this setup. `All_in_One_Weird` is
deprecated (see its `DEPRECATED.md`).

## Flashing a machine

Everything that differs between machines is in **one file:
`All_in_One/config.h`**. You never edit anything else.

### One-time setup on the computer (Windows)

1. Download **arduino-cli** for Windows
   (https://arduino.github.io/arduino-cli/latest/installation/) and put
   `arduino-cli.exe` next to `flash.bat` in this folder.
2. If Windows doesn't show a COM port when the board is plugged in, install
   the USB driver for the board's USB chip (CP210x or CH340).

The first flash downloads the ESP32 board package (version 2.0.17), which
takes a few minutes. Nothing else needs installing: the libraries are in
`All_in_One/src`.

### For each machine

1. **Check the machine in the VMS dashboard** and copy its machine ID exactly.
2. **Edit `All_in_One/config.h`:**

   | Setting | What to put |
   |---|---|
   | `MACHINE_ID` | the machine ID from the dashboard, e.g. `"JJMBRM160"` |
   | `MACHINE_TYPE` | `TYPE_STANDARD`, or `TYPE_TWO_GROUP_160` for the BELT/PANTY machine |
   | `MACHINE_SIZE` | `20`, `40`, `60`, `120` or `160` |
   | `NETWORK` | `NETWORK_GSM` (SIM card) or `NETWORK_WIFI` |
   | `WIFI_SSID`, `WIFI_PASSWORD` | the site's Wi-Fi name and password (Wi-Fi only; `""` on GSM) |
   | `HAS_RFID`, `HAS_BKASH`, `HAS_CASH` | `1` if the machine has that payment, `0` if not |
   | `SERVER` | `SERVER_PRODUCTION` for machines in the field |

3. **Plug in the board** and run, in this folder:
   ```
   flash.bat COM5
   ```
   (`flash.bat` on its own lists the boards and their COM ports.)
4. **Check the settings it shows** and press `Y`.
5. **Watch the log that follows.** A good start looks like:
   ```
   Firmware:      2.0.0 STD Nov 27 2026 10:15:00
   Machine ID:    JJMBRM160
   ...
   MQTT broker Connected!
   Published status on topic: vms/JJMBRM160/status
   ```
   The machine then shows as online in the dashboard. Close the window when
   done.

### If the build stops with an error

The build refuses to make a firmware with wrong settings, and the message
names the setting:

| Message contains | Fix |
|---|---|
| `set MACHINE_ID` | `MACHINE_ID` is still `"CHANGE_ME"` |
| `MACHINE_ID may only contain` | the ID has a space or another character; copy it exactly |
| `must all be there` | a setting is missing or misspelled; compare with the table above |
| `must be 20, 40, 60, 120 or 160` | `MACHINE_SIZE` |
| `WIFI_SSID is empty` | Wi-Fi machine without a network name |
| `TYPE_TWO_GROUP_160 can't have HAS_CASH 1` | the two-group machine has no cash acceptor |
| `Install ESP32 board package 2.0.17` | Arduino IDE only: install that exact version |

### If the machine doesn't connect

The log says why:

- `Could not connect to GSM` / `wifi`: SIM, signal, or Wi-Fi name/password.
- `MQTT connect failed, state 4` or `5`: the machines' login in
  `All_in_One/broker.h` doesn't match the broker.
- `MQTT connect failed, state -2`: the broker `iot-mqtt.headless.ltd`
  couldn't be reached, or the secure (TLS) connection failed.
- Connected, but the dashboard shows nothing: the machine ID differs from
  the dashboard, or `SERVER` is wrong.

### Arduino IDE instead of flash.bat

Install **ESP32 board package 2.0.17** (Tools > Board > Boards Manager >
"esp32" by Espressif > 2.0.17). Open `All_in_One/All_in_One.ino`, edit the
`config.h` tab, choose board **ESP32 Dev Module** (default options) and the
port, and press Upload. Open the Serial Monitor at 115200 baud to see the
log. No libraries need installing.

## Testing (software team)

- `tools/build-all.sh` builds every machine type and checks that `config.h`
  mistakes are rejected. Run it before every commit.
- `tools/build.sh -p <port> -m` builds, flashes and shows the log (macOS/Linux).
- To test against the staging server, set `SERVER SERVER_STAGING`: the
  machine uses the `/dev` topics and appears on `vms-new.headless.ltd`
  (the machine ID must exist in the staging database).

How the firmware works, and what not to change, is in `AGENTS.md`.
