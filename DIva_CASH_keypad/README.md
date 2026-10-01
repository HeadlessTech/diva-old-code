# DIva CASH Keypad

Firmware for a banknote-operated vending machine built on an **ESP32** (Arduino framework). Customers pay with cash notes and receive a product from one of three spring-motor columns; a 4x4 matrix keypad lets an admin unlock maintenance dispenses and change the access PIN.

## Features

- **Cash vending flow** — a pulse-based note acceptor counts inserted notes; once credit reaches the price, the machine dispenses a product automatically.
- **Composite video output (NTSC)** — full-screen status images (Welcome, Insert 10 Taka, Note Accepted, Dispensing, Collect Product, Please Refill, Timeout) on a composite display via the `esp_8_bit`-style driver.
- **SH1106 OLED (128x64, I2C)** — shows the admin PIN entry screen and feedback messages.
- **Admin keypad (PCF8574)** — 4-digit PIN entry, maintenance dispense, and PIN change mode.
- **EEPROM persistence** — dispense position/counters and the admin PIN survive power cycles.
- **Round-robin columns** — 3 columns x 20 products each; when a column empties, the next one is used; when all are empty, the machine shows "Please Refill".

## Hardware & Wiring

| Function | GPIO |
|---|---|
| Motor 1 / 2 / 3 (relay) | 23 / 4 / 2 |
| Limit switch 1 / 2 / 3 | 19 / 18 / 13 |
| Push button (start vend) | 39 |
| Reset button (hold ~3 s) | 36 |
| Note acceptor TX / RX | 14 / 35 |
| Composite video out | 25 |
| I2C SDA / SCL (keypad @0x20, OLED @0x3C) | 21 / 22 |

Keypad layout (`A` acts as OK/confirm):

```
1 4 7 *
2 5 8 0
3 6 9 #
A B C D
```

## Configuration

Edit the defines at the top of `DIva_CASH_keypad.ino`:

- `price` — credit (number of notes) required per vend (default `30`, each note = 1 pulse, i.e. a 10-Taka note)
- `totalColumn` — number of product columns (default `3`)
- `mProduct_in_Column` — products per column (default `20`)
- `maxPressTime` — note acceptor timeout in ms (default `60000`)

## How to Use

### Customer

1. Press and hold the **push button** — the display shows "Insert 10 Taka".
2. Insert notes; the acceptor stays on for up to 60 s after the last note.
3. When enough credit is reached, the acceptor stops, the machine dispenses the product, and the display shows "Collect Product".
4. If the timeout expires first, the display shows "Timeout" and credit is discarded.

### Admin

- **Maintenance dispense** — type the 4-digit PIN, then press `A`. On success the machine dispenses one product without payment. Default PIN is **0000**.
- **Change PIN** — type `*6969#`, then enter a new 4-digit PIN. The new PIN is stored in EEPROM.
- **Reset counters** — hold the **reset button** for ~3 s. Resets to column 1 with zero counts (also used to exit the "Please Refill" state after restocking).

## Building & Flashing

- Open `DIva_CASH_keypad.ino` in the Arduino IDE (or PlatformIO) with an ESP32 board selected, then build and flash.
- Library dependencies: `Adafruit_GFX`, `Adafruit_SH110X`, `PCF8574`, `EEPROM`, `Wire`.
- `debug.cfg` and `esp32.svd` are provided for OpenOCD/JTAG debugging on ESP32.

## File Overview

- `DIva_CASH_keypad.ino` — main state machine, vend/dispense logic, EEPROM counters
- `Keypad.h` — PCF8574 keypad scanning, PIN entry/change, EEPROM PIN storage
- `OLED.h` — SH1106 OLED setup
- `CompositeGraphics.h` / `CompositeColorOutput.h` / `Color.h` — NTSC composite video output and drawing
- `*.h` image assets (`welcome`, `insertTenTaka`, `dispensing`, etc.) — screens shown on the composite display
