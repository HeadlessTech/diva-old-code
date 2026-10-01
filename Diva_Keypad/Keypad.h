#pragma once

#include <Wire.h>
#include <PCF8574.h>
#include <EEPROM.h>  // for PIN storage
#include <cstring>  // strlen, strcmp
#include "OLED.h"  // display feedback for PIN entry

// EEPROM size used for this project (make sure > 130 for PIN)
#ifndef eeprSize
#define eeprSize 256  // EEPROM Size in bytes (was 128, expanded for PIN)
#endif

// EEPROM address for storing 4‑digit admin PIN
#define eeprAddrPin 130
#define eeprPinSize 4

// keys used as terminators
#define KEY_OK 'A'            // end‑of‑PIN / confirm
constexpr const char* CHANGE_SEQ = "*5858#";  // sequence to enter PIN‑change mode

// buffer for keypad sequence detection
#define SEQ_BUF_SIZE 8U

static char keyBuf[SEQ_BUF_SIZE];
static int keyBufLen = 0;

// current stored PIN (null terminated)
static char currentPin[5] = "0000";

// defined in Diva_Keypad.ino; used to show remaining stock on the OLED
extern int dispenseCount;

// buffer shown on OLED while user types
static char displayPin[5] = "";
static int displayPinLen = 0;

// forward declarations of PIN/keypad routines (definitions follow)
void loadPin();
void savePin(const char* newPin);
void handleKeypadInput();
bool checkChangeSequence();
bool tryAdminUnlock();
void enterPinChangeMode();
void dispenseAdmin();

// OLED helpers for PIN entry feedback
static void clearOled() {
  oled.clearDisplay();
  oled.display();
}

// Show a one- or two-line centred message (no PIN, no hint).
// Used for transient feedback: "Wrong PIN!", "Access OK!", etc.
static void showOledMessage(const char* line1, const char* line2 = nullptr) {
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  oled.setTextSize(1);
  if (line2 && strlen(line2) > 0) {
    int x1 = ((int) SCREEN_WIDTH - (int) strlen(line1) * 6) / 2;
    if (x1 < 0) x1 = 0;
    oled.setCursor(x1, 22);
    oled.print(line1);
    int x2 = ((int) SCREEN_WIDTH - (int) strlen(line2) * 6) / 2;
    if (x2 < 0) x2 = 0;
    oled.setCursor(x2, 36);
    oled.print(line2);
  } else {
    int x1 = ((int) SCREEN_WIDTH - (int) strlen(line1) * 6) / 2;
    if (x1 < 0) x1 = 0;
    oled.setCursor(x1, 28);
    oled.print(line1);
  }
  oled.display();
}

// Show the PIN-entry screen.
//   msg  – title line    (default "Enter PIN")
//   hint – footer line   (default "Press [A] to confirm")
// The four digit slots reflect the live displayPin buffer:
//   filled slots show the typed digit, empty slots show '-'.
// Because displayPin is a rolling 4-char buffer this always
// shows exactly the last four digits the user has typed.
// Top-left corner shows the remaining stock ("S:152").
static void showPinPrompt(const char* msg = "Enter PIN",
  const char* hint = "Press [A] to confirm") {
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  oled.setTextSize(1);

  // ── Stock counter (top-left, size 1) ───────────────────────
  // max "S:160" = 5 chars = 30 px; the centred title starts at
  // x >= 31 for any title up to 11 chars, so they never overlap.
  int stock = TOTAL_PRODUCTS - dispenseCount;
  if (stock < 0) stock = 0;
  oled.setCursor(0, 0);
  oled.print("S:");
  oled.print(stock);

  // ── Title (size 1, centred) ────────────────────────────────
  int titleX = ((int) SCREEN_WIDTH - (int) strlen(msg) * 6) / 2;
  if (titleX < 0) titleX = 0;
  oled.setCursor(titleX, 0);
  oled.print(msg);

  // ── Divider ───────────────────────────────────────────────
  oled.drawFastHLine(0, 11, SCREEN_WIDTH, SH110X_WHITE);

  // ── PIN slots (size 2: 12 px wide × 16 px tall per char) ──
  // Layout: "X X X X"  4 chars + 3 gaps of 4 px = 60 px wide, centred
  oled.setTextSize(2);
  const int slotW = 12;
  const int gap = 4;
  const int pinW = 4 * slotW + 3 * gap;   // 60 px
  const int pinX = ((int) SCREEN_WIDTH - pinW) / 2;  // ~34 px
  const int pinY = 16;
  for (int i = 0; i < 4; i++) {
    oled.setCursor(pinX + i * (slotW + gap), pinY);
    oled.print(i < displayPinLen ? displayPin[i] : '-');
  }

  // ── Footer hint (size 1, centred) ─────────────────────────
  oled.setTextSize(1);
  int hintX = ((int) SCREEN_WIDTH - (int) strlen(hint) * 6) / 2;
  if (hintX < 0) hintX = 0;
  oled.setCursor(hintX, 55);
  oled.print(hint);

  oled.display();
}

static void appendDisplayDigit(char d) {
  if (displayPinLen < 4) {
    displayPin[displayPinLen++] = d;
  } else {
    // shift left
    for (int i = 1; i < 4; i++) displayPin[i - 1] = displayPin[i];
    displayPin[3] = d;
  }
  displayPin[displayPinLen] = '\0';
}

static void clearDisplayPin() {
  displayPinLen = 0;
  displayPin[0] = '\0';
}

// helper utilities
static void addKey(char k) {
  if (keyBufLen < SEQ_BUF_SIZE) {
    keyBuf[keyBufLen++] = k;
  } else {
    for (uint8_t i = 1; i < SEQ_BUF_SIZE; i++)
      keyBuf[i - 1] = keyBuf[i];
    keyBuf[SEQ_BUF_SIZE - 1] = k;
  }
}

static bool endsWith(const char* s, int len, const char* suffix, int slen) {
  if (len < slen) return false;
  for (int i = 0; i < slen; i++) {
    if (s[len - slen + i] != suffix[i]) return false;
  }
  return true;
}

PCF8574 pcf(0x20);

// keypad layout; 'A' is repurposed as PIN confirm (OK)
const char keys[4][4] = {
  {'1', '4', '7', '*'},
  {'2', '5', '8', '0'},
  {'3', '6', '9', '#'},
  {'A', 'B', 'C', 'D'}
};

// Bit masks
const uint8_t ROW_MASK = 0x0F;  // P0-P3
const uint8_t COL_MASK = 0xF0;  // P4-P7

void keypadInit() {
  Wire.begin(16, 17);     // SDA = GPIO16, SCL = GPIO17
  Wire.setClock(400000);  // Fast I2C 400kHz
  if (!pcf.begin()) {
    Serial.println("PCF8574 not found!");
    while (1);
  }
  // Initial state: rows LOW, columns HIGH (input pullups)
  pcf.write8(0xF0);
  Serial.println("Keypad ready!");
}

char scanKeypad() {
  for (int row = 0; row < 4; row++) {
    // Create row pattern: one row LOW, others HIGH
    // Columns kept as HIGH (input with pullup)
    uint8_t output = 0xF0 | ~(1 << row);
    pcf.write8(output);

    delayMicroseconds(100);

    uint8_t input = pcf.read8();
    uint8_t cols = input & COL_MASK;

    if (cols != COL_MASK) {  // If any column is LOW
      int col = -1;
      if (!(cols & 0x10)) col = 0;      // P4
      else if (!(cols & 0x20)) col = 1; // P5
      else if (!(cols & 0x40)) col = 2; // P6
      else if (!(cols & 0x80)) col = 3; // P7

      if (col != -1) {
        return keys[row][col];
      }
    }
  }

  return 0;
}

// PIN-related implementation details

void loadPin() {
  EEPROM.begin(eeprSize);
  delay(100);
  bool valid = true;
  for (int i = 0; i < eeprPinSize; i++) {
    char c = EEPROM.read(eeprAddrPin + i);
    if (c < '0' || c > '9') {
      valid = false;
      break;
    }
    currentPin[i] = c;
  }
  EEPROM.end();
  currentPin[eeprPinSize] = '\0';

  if (!valid) {
    // no valid PIN stored yet, initialize to "0000"
    strcpy(currentPin, "0000");
    savePin(currentPin);
    Serial.println("Default PIN written (0000)");
  }
  Serial.print("Current PIN: ");
  Serial.println(currentPin);
}

void savePin(const char* newPin) {
  EEPROM.begin(eeprSize);
  delay(100);
  for (int i = 0; i < eeprPinSize; i++) {
    EEPROM.write(eeprAddrPin + i, newPin[i]);
  }
  EEPROM.commit();
  EEPROM.end();
  strncpy(currentPin, newPin, eeprPinSize);
  currentPin[eeprPinSize] = '\0';
}

bool checkChangeSequence() {
  int slen = strlen(CHANGE_SEQ);
  if (keyBufLen >= slen && endsWith(keyBuf, keyBufLen, CHANGE_SEQ, slen)) {
    return true;
  }
  return false;
}
