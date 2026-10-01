# DIva Keypad

Firmware for a **PIN-operated** vending machine built on an **ESP32** (Arduino framework). Every dispense is gated by an admin 4-digit PIN entered on a 4x4 matrix keypad — there is no push button, no cash acceptor, no payment flow.

This project is derived from `DIva_CASH_keypad` with all cash and push-button logic removed; the admin keypad, EEPROM persistence, dispense motor control, and reset behavior are preserved unchanged.

## Features

- **PIN-only dispense** — type the 4-digit admin PIN and press `A`; the machine dispenses one product from the current column.
- **SH1106 OLED (128x64, I2C)** — the only display; shows the admin PIN entry screen and all status/feedback messages (dispensing, collect product, please refill, reset done, etc).
- **Admin keypad (PCF8574)** — PIN entry, maintenance dispense, and PIN change mode (`*6969#`).
- **EEPROM persistence** — dispense position/counters and the admin PIN survive power cycles; a blank/erased EEPROM (fresh chip) is detected automatically and initialized to safe defaults instead of reading garbage.
- **Round-robin columns** — machine capacity is set by a single `MACHINE_SIZE` constant (default `160`), split into columns of 20 products each; when a column empties, the next one is used; when all are empty, the machine shows "Please Refill".
- **Motor stall protection** — each dispense motor is cut off after a fixed timeout if its limit switch never trips, instead of running indefinitely on a mechanical or wiring fault.

## Hardware & Wiring

Machine capacity (`MACHINE_SIZE`) is `160`, giving 8 columns of 20 products (`MACHINE_SIZE / 20`). Each pair of columns shares one limit switch input:

| Column | Motor GPIO | Limit switch GPIO |
|---|---|---|
| 1 | 23 | 39 |
| 2 | 25 | 39 |
| 3 | 4  | 19 |
| 4 | 2  | 19 |
| 5 | 32 | 18 |
| 6 | 33 | 18 |
| 7 | 26 | 13 |
| 8 | 27 | 13 |

> GPIO 33 (column 6) doubles as a crystal-oscillator input on some ESP32 modules; the firmware re-asserts `OUTPUT` mode on it immediately before driving that motor to work around timer peripherals that can silently flip it back to `INPUT`.

| Function | GPIO |
|---|---|
| Reset button (long press) | 36 |
| I2C SDA / SCL (keypad @0x20, OLED @0x3C) | 16 / 17 |

- The reset button (GPIO 36) and the column 1/2 limit switch (GPIO 39) both sit on the ESP32's input-only pins (34-39), which have **no internal pull resistor** — both need an **external pull-up resistor** wired in, which is already in place for the reset button.
- Reset detection is edge/hold-based via the `PinButton` library rather than a hand-rolled hold timer, configured to require a 2-second hold (see the *Reset counters* section below).
- CPU frequency is explicitly set to 240 MHz in `setup()`.

Keypad layout (`A` acts as OK/confirm):

```
1 4 7 *
2 5 8 0
3 6 9 #
A B C D
```

## Configuration

- `MACHINE_SIZE` (top of `Diva_Keypad.ino`) — total product capacity; columns are derived as `MACHINE_SIZE / 20`. Must stay a multiple of 20, and the resulting column count must not exceed the 8 motor/4 limit-switch pins wired in `Dispense.h`.
- `TEST` (top of `Diva_Keypad.ino`, commented out by default) — uncomment `#define TEST 1` to build a fast-test firmware where every column holds **1** product instead of 20 (`TOTAL_PRODUCTS` = 8). Column count and pin mapping are unchanged. Counters stored by the other build are detected as out-of-range on boot and reset to defaults automatically. Comment it out again for the production build.
- `LIMIT_SWITCH_TIMEOUT_MS` (`Dispense.h`, inside `runMotorSpring`) — how long a motor is allowed to run while waiting for its limit switch before being force-stopped (default 8 seconds).
- `RESET_BUTTON_CONFIG` (`Diva_Keypad.ino`, the `MultiButtonConfig` passed to `resetButton`) — `{ debounceDecay, singleClickDelay, longClickDelay }` in ms; `longClickDelay` (default `2000`) is how long the reset button must be held to trigger a reset.

## How to Use

### Dispense (admin)

1. Type the 4-digit PIN on the keypad — digits fill the OLED slots (rolling buffer, only the last 4 typed digits are shown).
2. Press **`A`** to confirm. The digits are wiped from the screen before checking.
3. **Correct PIN** → OLED shows "Access OK! Dispensing..." → one product is dispensed from the current column → "Dispense done / Collect your product" (shown for 1 second) → returns to the PIN prompt. The dispense counter and round-robin column advance and persist in EEPROM.
4. **Wrong PIN** → "Wrong PIN!" message; keypad buffer is cleared; retype from scratch.

If a dispense motor's limit switch never trips (jam, wiring fault, mechanical failure), the motor is automatically cut after 8 seconds rather than running indefinitely — check `Serial` output for a `TIMEOUT` log line to diagnose which column failed.

### Change PIN

1. Type `*6969#` at any time at idle (no existing PIN required — this is intentional, by design).
2. OLED switches to "Set New PIN / Enter 4 digits".
3. Type exactly 4 digits within **30 seconds**. This intentionally blocks everything else on the machine (reset button included) — there's nothing else useful for it to do mid-entry.
4. Success → saved to EEPROM, "PIN Saved!" shown for 2 s. Timeout or incomplete → "Timed out! PIN unchanged".
5. Returns to the normal "Enter PIN" prompt.

### Reset counters (after restocking)

- Press and hold the **reset button** for at least **2 seconds** to trigger the long click (via the `PinButton`/`MultiButton` library, configured with a 2000 ms `longClickDelay`). OLED shows "Reset Done / Counters cleared". Resets to column 1 with zero counts; also used to exit the "Please Refill" state once the machine is refilled.

Default admin PIN on first boot: **`0000`** (written to EEPROM address 130).

> If this firmware is flashed on an ESP32 that previously ran `DIva_CASH_keypad`, the stored PIN and counters carry over (same EEPROM layout).

## Building & Flashing

- Open `Diva_Keypad.ino` in the Arduino IDE (or PlatformIO) with an ESP32 board selected, then build and flash.
- Library dependencies: `Adafruit_GFX`, `Adafruit_SH110X`, `PCF8574`, `EEPROM`, `Wire`, `PinButton` (`MultiButton`).
- `debug.cfg` and `esp32.svd` are provided for OpenOCD/JTAG debugging on ESP32.

## File Overview

- `Diva_Keypad.ino` — main state machine, EEPROM counters, reset/refill handling, admin PIN flow
- `Dispense.h` — motor/limit-switch pin maps and the motor-drive routine (`dispenseProcess`, `runMotorSpring`, stall timeout)
- `Keypad.h` — PCF8574 keypad scanning, PIN entry/change, EEPROM PIN storage
- `OLED.h` — SH1106 OLED setup and all on-screen feedback (the machine's only display)
