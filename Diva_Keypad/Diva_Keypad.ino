#define MACHINE_SIZE 160   // physical capacity: 20 products per column -> 8 columns

#define TEST 1   // uncomment: every column holds only 1 product (fast test cycle)

#define PRODUCT_IN_COLUMN_FULL 20
#ifdef TEST
  #define PRODUCT_IN_COLUMN 1
#else
  #define PRODUCT_IN_COLUMN PRODUCT_IN_COLUMN_FULL
#endif
#define TOTAL_COLUMNS  (MACHINE_SIZE / PRODUCT_IN_COLUMN_FULL)   // column count never changes with TEST
#define TOTAL_PRODUCTS (TOTAL_COLUMNS * PRODUCT_IN_COLUMN)       // what "sold out" is measured against

#include "esp_pm.h"
#include <PinButton.h>

#include "Keypad.h"  // keypad helper for admin PIN input
#include "Dispense.h"

#define eeprAddr1 100  //EEPROM Address where column number will store
#define eeprAddr2 110
#define eeprAddr3 120

int column = 0;
int dispenseCount = 0;
int columnDispenseCount = 0;

void EEPRomRead() {
  EEPROM.begin(eeprSize);
  delay(50U);
  uint32_t colRaw = (uint32_t)EEPROM.readInt(eeprAddr1);
  uint32_t dCountRaw = (uint32_t)EEPROM.readInt(eeprAddr2);
  uint32_t cdCountRaw = (uint32_t)EEPROM.readInt(eeprAddr3);
  EEPROM.end();
  delay(50U);

  bool isBlank = (colRaw == 0xFFFFFFFF) || (dCountRaw == 0xFFFFFFFF) || (cdCountRaw == 0xFFFFFFFF);
  // counters left by a build with a different PRODUCT_IN_COLUMN (e.g. TEST <-> normal)
  bool outOfRange = (colRaw < 1) || (colRaw > TOTAL_COLUMNS)
                 || (cdCountRaw >= PRODUCT_IN_COLUMN) || (dCountRaw > TOTAL_PRODUCTS);
  if (isBlank || outOfRange) {
    column = 1;
    dispenseCount = 0;
    columnDispenseCount = 0;
    Serial.println(isBlank ? "Blank EEPRom detected, initializing to defaults"
                           : "EEPRom counters out of range for this build, resetting to defaults");
    EEPRomwrite(column, dispenseCount, columnDispenseCount);
  } else {
    column = colRaw;
    dispenseCount = dCountRaw;
    columnDispenseCount = cdCountRaw;
    Serial.println("Read from EEPRom");
  }

  Serial.print("Column value = ");
  Serial.print(column);
  Serial.print(" & Dispense Count = ");
  Serial.print(dispenseCount);
  Serial.print(" & Column Dispense Count = ");
  Serial.println(columnDispenseCount);
  delay(50U);
}
void EEPRomwrite(uint32_t col, uint32_t dCount, uint32_t cdCount) {
  EEPROM.begin(eeprSize);
  delay(50U);
  EEPROM.writeInt(eeprAddr1, col);
  delay(10U);
  EEPROM.writeInt(eeprAddr2, dCount);
  delay(10U);
  EEPROM.writeInt(eeprAddr3, cdCount);
  delay(10U);
  EEPROM.commit();
  EEPROM.end();
  Serial.print("Column value = ");
  Serial.print(col);
  Serial.print("Dispense count= ");
  Serial.print(dCount);
  Serial.print("Column Dispense count= ");
  Serial.print(cdCount);
  Serial.println(" Write to EEPRom");
}

// Require a deliberate 2s hold before the reset long-click fires
// (library default is only 300ms, too easy to trigger by accident).
constexpr MultiButtonConfig RESET_BUTTON_CONFIG = { 20, 250, 2000 };
PinButton resetButton(GPIO_NUM_36, &RESET_BUTTON_CONFIG);


void dispensePostProcessing() {
  columnDispenseCount = columnDispenseCount + 1;
  dispenseCount = dispenseCount + 1;
  if (columnDispenseCount >= PRODUCT_IN_COLUMN) {
    columnDispenseCount = 0;
    column = column + 1;
  }

  if (column > TOTAL_COLUMNS) {
    column = 1;
  }

  EEPRomwrite(column, dispenseCount, columnDispenseCount);     //Update column/row number to EEPRom
  Serial.println("Post Processing Done!");
}

void checkResetButton() {
  resetButton.update();
  if (resetButton.isLongClick() == 1) {
    Serial.println("Long pressed reset button");
    column = 1;
    dispenseCount = 0;
    columnDispenseCount = 0;
    delay(250U);
    EEPRomwrite(column, dispenseCount, columnDispenseCount);
    showOledMessage("Reset Done", "Counters cleared");
    Serial.println("Reset Done");
    delay(2000U);
    EEPRomRead();
    clearDisplayPin();
    showPinPrompt();
  }
}

void setup() {
  setCpuFrequencyMhz(240);
  Serial.begin(115200);

  Serial.println("Diva Keypad machine");
  Serial.print("Machine size: ");
  Serial.println(MACHINE_SIZE);
#ifdef TEST
  Serial.printf("*** TEST MODE: %d columns x %d product = %d total ***\n",
                TOTAL_COLUMNS, PRODUCT_IN_COLUMN, TOTAL_PRODUCTS);
#endif

  for (auto limitSwitch : motorLimitSwitches) {
    pinMode(limitSwitch, INPUT_PULLUP);
  }

  for (auto motorPin : motorPins) {
    pinMode(motorPin, OUTPUT);
    delay(25U);
    digitalWrite(motorPin, LOW);
  }
  
  keypadInit();              // prepare keypad hardware
  loadPin();                 // read or initialize admin PIN

  EEPRomRead();        //Get the Column/Row number to be dispensed after a restart

  setupOled();
  delay(1500);
  showPinPrompt();           // show "Enter PIN" on the OLED at startup
}

bool tryAdminUnlock() {
  if (keyBufLen < 5 || keyBuf[keyBufLen - 1] != KEY_OK) return false;
  char attempt[5];
  for (int i = 0; i < 4; i++) {
    char c = keyBuf[keyBufLen - 5 + i];
    if (c < '0' || c > '9') return false;
    attempt[i] = c;
  }
  attempt[4] = '\0';
  if (strcmp(attempt, currentPin) == 0) {
    Serial.println("Admin PIN correct");
    return true;
  } else {
    Serial.print("Admin PIN wrong: ");
    Serial.println(attempt);
    keyBufLen = 0;
    return false;
  }
}

void enterPinChangeMode() {
  Serial.println("Entering PIN change mode, waiting for 4 digits...");
  clearDisplayPin();
  showPinPrompt("Set New PIN", "  Enter 4 digits  ");

  unsigned long start = millis();
  while (millis() - start < 30000UL && displayPinLen < 4) {
    char k = scanKeypad();
    if (k) {
      if (k >= '0' && k <= '9') {
        appendDisplayDigit(k);
        showPinPrompt("Set New PIN", "  Enter 4 digits  ");
      }
      delay(200);
    }
  }

  if (displayPinLen == 4) {
    savePin(displayPin);
    Serial.print("New PIN stored: ");
    Serial.println(displayPin);
    clearDisplayPin();
    showOledMessage("PIN Saved!", "PIN updated OK");
    delay(2000);
  } else {
    Serial.println("PIN change timed out or incomplete");
    showOledMessage("Timed out!", "PIN unchanged");
    delay(1500);
  }

  keyBufLen = 0;
  clearDisplayPin();
  showPinPrompt();       // restore normal "Enter PIN" prompt
}

void handleKeypadInput() {
  char k = scanKeypad();
  if (!k) return;

  Serial.print("Keypad pressed: ");
  Serial.println(k);
  addKey(k);

  // check for change sequence first (may contain digits and '#')
  if (checkChangeSequence()) {
    clearDisplayPin();
    clearOled();
    enterPinChangeMode();
    return;
  }

  if (k >= '0' && k <= '9') {
    // rolling buffer: only the last 4 typed digits are shown
    appendDisplayDigit(k);
    showPinPrompt();
  } else if (k == KEY_OK) {
    // Privacy: wipe digits from screen immediately, before checking
    clearDisplayPin();
    showPinPrompt();   // all slots now show '-'

    if (tryAdminUnlock()) {
      showOledMessage("Access OK!", "Dispensing...");
      delay(800);
      dispenseAdmin();   // restores OLED to "Enter PIN" at its end
    } else {
      showOledMessage("Wrong PIN!", "Try again...");
      delay(1500);
      showPinPrompt();   // back to clean "Enter PIN" prompt
    }
    return;   // skip debounce below; response was already slow enough
  }

  delay(200);  // debounce for digit and other keys
}

// dispense same as regular flow but triggered via keypad
void dispenseAdmin() {
  dispenseProcess(column);
  dispensePostProcessing();

  showOledMessage("Dispense done", "Collect your product");
  delay(1000U);

  // clear buffer so additional KEY_OK doesn't trigger again
  keyBufLen = 0;
  // restore OLED to the PIN entry prompt
  clearDisplayPin();
  showPinPrompt();
}


void loop() {
  if (dispenseCount >= TOTAL_PRODUCTS) {
    showOledMessage("No Product!", "Please refill");
    Serial.println("product sesh.");

    while (true) {
      checkResetButton();
      delay(1U);
      if (dispenseCount < TOTAL_PRODUCTS) break;
    }
  }
  
  checkResetButton();

  handleKeypadInput();
}
