// comment this line on production 
// #define IS_DEVELOPMENT

// Offline operation: cash and RFID without a network/backend.

#define MACHINE_ID "BAIO20"
#define MACHINE_SIZE 60

#define HAS_RFID_MODULE
#define HAS_CASH_MODULE

#include <PinButton.h>

#if defined(HAS_RFID_MODULE)
#include <rdm6300.h>
Rdm6300 rdm6300;
#endif

unsigned long lastRfidTime = 0;
// Keep the same 5 s RFID cooldown through local dispensing.
const unsigned long RFID_DEBOUNCE_MS = 5000;

#include "Global.h"
#include "Dispense.h"
#include "display.h"
#include "Offline.h"
#if defined(HAS_CASH_MODULE)
#include "FeatureCash.h"
#endif

PinButton pushButton(GPIO_NUM_39);
PinButton resetButton(GPIO_NUM_36);

void checkPushButton() {
#ifdef HAS_CASH_MODULE
  GLOBAL_STATE currBtnState;
  getGlobalState(currBtnState);
  if (currBtnState != GLOBAL_STATE::IDLE || !globalData.hasStock) return;
  // Match DIva_CASH_160's active-LOW button handling.
  int pressedReads = 0;
  while (digitalRead(GPIO_NUM_39) == LOW) {
    if (++pressedReads >= 50) {
      Serial.println("Pressed push button");
      turnOnCashAcceptor();
      break;
    }
  }
#endif
}

void checkResetButton() {
  int heldIntervals = 0;
  while (digitalRead(GPIO_NUM_36) == LOW) {
    waitOffline(100U);
    if (++heldIntervals > 30) {
      Serial.println("Long pressed reset button: stock refilled");
      setGlobalState(GLOBAL_STATE::SYNC);
      refillOfflineStock();
      showImage(syncComplete_t);
      waitOffline(2000U);
      loadOfflineStock();
      flushRfidReader();
      showOfflineStatus();
      break;
    }
  }
}
unsigned long monitorStart = millis();
unsigned long monitorInterval = 15000UL;
void monitorHealth() {
#if defined(IS_DEVELOPMENT)
  unsigned long currentMillis = millis();
  if (currentMillis - monitorStart >= monitorInterval) {
    Serial.printf("\n[System Monitor] Free heap at runtime: %.2f KB\n\n", ESP.getFreeHeap() / 1024.0);
    monitorStart = currentMillis;
  }
#endif
}

void setup() {
  setCpuFrequencyMhz(240);
  Serial.begin(115200);

  delay(500);

  // ---- Build config (one-shot boot log) ----
  Serial.println();
  Serial.println("========================================");
  Serial.printf("Machine ID:    %s\n", MACHINE_ID);
  Serial.printf("Machine size:  %d\n", MACHINE_SIZE);
  Serial.printf("Build mode:    %s\n",
#ifdef IS_DEVELOPMENT
    "DEVELOPMENT"
#else
    "PRODUCTION"
#endif
  );
  Serial.println("Network:      OFFLINE");
  Serial.println("Modules:");
  Serial.printf("  RFID:  %s\n",
#ifdef HAS_RFID_MODULE
    "ENABLED"
#else
    "disabled"
#endif
  );
  Serial.printf("  CASH:  %s\n",
#ifdef HAS_CASH_MODULE
    "ENABLED"
#else
    "disabled"
#endif
  );

  delay(200U);
  Serial.printf("CPU Freq: %d MHz\n", getCpuFrequencyMhz());
  Serial.printf("XTAL Freq: %d MHz\n", getXtalFrequencyMhz());
  Serial.printf("Reset reason: %d\n", esp_reset_reason());

  Serial.println("========================================");
  Serial.println();

#if defined(HAS_CASH_MODULE)
  pinMode(NOTE_ACCEPTOR_RX, INPUT);  // GPIO 35 requires an external pull-up.
  pinMode(NOTE_ACCEPTOR_TX, OUTPUT);

  turnOffCashAcceptor();
#endif

  // GPIO 36/39 also require external pull-ups; both buttons are active-LOW.
  pinMode(GPIO_NUM_39, INPUT);
  pinMode(GPIO_NUM_36, INPUT);

  for (auto limitSwitch : motorLimitSwitches) {
    pinMode(limitSwitch, INPUT_PULLUP);
  }

  for (auto motorPin : motorPins) {
    pinMode(motorPin, OUTPUT);
    delay(30U);
    digitalWrite(motorPin, LOW);
  }

  delay(100U);

  composite.init();
  graphics.init();
  showImage(welcome_t);
  delay(2000U);

  loadOfflineStock();

  Serial.println("Now we loop.");
  showOfflineStatus();

#if defined(HAS_RFID_MODULE)
  rdm6300.begin(GPIO_NUM_5);
#endif
  
  // Service RFID during motor movement without processing another dispense.
  setRfidPollCallback(discardRfidReads);

#if defined(HAS_RFID_MODULE)
  // make internal reader debounce match our constant (ms)
  rdm6300.set_tag_timeout(RFID_DEBOUNCE_MS);
  Serial.println("RFID listener started");
#endif
}


void loop() {
  monitorHealth();
  checkResetButton();

  if (digitalRead(GPIO_NUM_36) == LOW) {
    discardRfidReads();
    return;
  }

  if (!globalData.hasStock) {
    GLOBAL_STATE refillState;
    getGlobalState(refillState);
    if (refillState != GLOBAL_STATE::EMPTY_STOCK) {
      showOfflineStatus();
    }
    discardRfidReads();
    return;
  }

  GLOBAL_STATE currState;
  getGlobalState(currState);
  if (currState == GLOBAL_STATE::IDLE) {
    showModuleImage();
  }

  checkPushButton();

  // Cash may have used the final pad. Refresh state before accepting RFID.
  getGlobalState(currState);
  if (currState != GLOBAL_STATE::IDLE || !globalData.hasStock) return;

#if defined(HAS_RFID_MODULE)
  uint32_t currentTag = rdm6300.get_new_tag_id();
  if (currentTag != 0) {
    unsigned long currentTime = millis();
    if (currentTime - lastRfidTime > RFID_DEBOUNCE_MS) {
      showImage(rfidProcessing_t);
      Serial.print("RFID read: ");
      Serial.println(currentTag);
      lastRfidTime = currentTime;
      dispenseOffline();
    }
  }
#endif
}
