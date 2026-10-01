// comment this line on production 
// #define IS_DEVELOPMENT

// comment this line when using GSM
// #define USE_WIFI

#define MACHINE_ID "TR_1"
#define MACHINE_SIZE 160

#define HAS_BKASH_MODULE
#define HAS_RFID_MODULE
// #define HAS_CASH_MODULE

#include <ArduinoJson.h>
#include <PinButton.h>

#if defined(HAS_RFID_MODULE)
#include <rdm6300.h>
Rdm6300 rdm6300;
#endif

#include "Global.h"
#include "Client.h"
#include "MQTT.h"
#include "Dispense.h"
#include "RequestsResponse.h"
#if defined(HAS_CASH_MODULE)
#include "FeatureCash.h"
#endif
#include "display.h"

PinButton topButton(GPIO_NUM_39);
PinButton bottomButton(GPIO_NUM_36);

uint32_t lastRfidTag = 0;
unsigned long lastRfidTime = 0;
// 5 s cooldown: long enough to survive a motor run + sync round-trip so the
// card cannot re-trigger immediately after a dispense or after reconnect.
const unsigned long RFID_DEBOUNCE_MS = 5000UL;

static bool pongPending = false; // toggled by onMQTT handler

void checkTopButton() {
  // In dual-group mode, push button (GPIO39) = Top group selector
  topButton.update();

#if defined(IS_DEVELOPMENT)
  if (topButton.isLongClick() == 1) {
    // Long-press = sync/reconnect (replaces dedicated reset button)
    showImage(connecting_t);
    Serial.println("Long pressed top button - reconnect");
    pickBestClient();
    setupMqtt();
    sendSyncRequest(false);
    return;
  }
#endif

  if (topButton.isSingleClick() == 1) {
    GLOBAL_STATE currState;
    getGlobalState(currState);
    DispenseRequest currentDispReq;
    getDispenseRequest(currentDispReq);
    if (currState == GLOBAL_STATE::IDLE && !currentDispReq.isExpectingResponse) {
      Serial.println("Top button pressed - selecting top group");
      // Check local per-size stock before entering AWAITING_RFID
      if (getStockForSize(SIZE_BELT) == 0 && sizeStock.belt == 0 && sizeStock.panty == 0) {
        // could be sizeStock not populated yet (first sync); let it through
      } else if (getStockForSize(SIZE_BELT) == 0) {
        Serial.println("BELT stock empty - showing refill");
        showImage(refill_t);
        delay(2000U);
        showImage(pushBtnOrScanQR_t);
        return;
      }
      flushRfidReader();
      selectedGroup = SIZE_BELT;
      groupSelectedAt = millis();
      setGlobalState(GLOBAL_STATE::AWAITING_RFID);
      showImage(holdRfid_t);
    } else if (currState == GLOBAL_STATE::AWAITING_RFID) {
      // Switch group selection while already waiting
      Serial.println("Switching to top group");
      selectedGroup = SIZE_BELT;
      groupSelectedAt = millis();
    }
  }

#if defined(HAS_CASH_MODULE)
  if (topButton.isSingleClick() == 1 || topButton.isLongClick() == 1) {
    Serial.println("Pressed push button");
    bool isOk = checkServerCommunicationForCashAcceptor();
    if (isOk) {
      turnOnCashAcceptor();
    }
  }
#endif
}

void checkBottomButton() {
  bottomButton.update();

#if defined(IS_DEVELOPMENT)
  if (bottomButton.isLongClick() == 1) {
    // Long-press = sync request
    Serial.println("Long pressed bottom button - sync");
    sendSyncRequest(false);
    return;
  }
#endif

  if (bottomButton.isSingleClick() == 1) {
    GLOBAL_STATE currState;
    getGlobalState(currState);
    DispenseRequest currentDispReq;
    getDispenseRequest(currentDispReq);
    if (currState == GLOBAL_STATE::IDLE && !currentDispReq.isExpectingResponse) {
      Serial.println("Bottom button pressed - selecting bottom group");
      // Check local per-size stock before entering AWAITING_RFID
      if (getStockForSize(SIZE_PANTY) == 0 && sizeStock.belt == 0 && sizeStock.panty == 0) {
        // could be sizeStock not populated yet (first sync); let it through
      } else if (getStockForSize(SIZE_PANTY) == 0) {
        Serial.println("PANTY stock empty - showing refill");
        showImage(refill_t);
        delay(2000U);
        showImage(pushBtnOrScanQR_t);
        return;
      }
      flushRfidReader();
      selectedGroup = SIZE_PANTY;
      groupSelectedAt = millis();
      setGlobalState(GLOBAL_STATE::AWAITING_RFID);
      showImage(holdRfid_t);
    } else if (currState == GLOBAL_STATE::AWAITING_RFID) {
      // Switch group selection while already waiting
      Serial.println("Switching to bottom group");
      selectedGroup = SIZE_PANTY;
      groupSelectedAt = millis();
    }
  }

#if defined(HAS_CASH_MODULE)
  if (bottomButton.isSingleClick() == 1 || bottomButton.isLongClick() == 1) {
    Serial.println("Pressed bottom button");
    bool isOk = checkServerCommunicationForCashAcceptor();
    if (isOk) {
      turnOnCashAcceptor();
    }
  }
#endif
}

void checkButtons() {
  checkTopButton();
  checkBottomButton();
}


unsigned long monitorStart = millis();
unsigned long monitorInterval = 15000UL;
unsigned long syncStart = millis();
unsigned long syncInterval = 90000UL;
void monitorHealth() {
  unsigned long currentMillis = millis();
#if defined(IS_DEVELOPMENT)
  if (currentMillis - monitorStart >= monitorInterval) {
    Serial.printf("\n[System Monitor] Free heap at runtime: %.2f KB\n\n", ESP.getFreeHeap() / 1024.0);
    monitorStart = currentMillis;
  }
#endif
 
  if (currentMillis - syncStart >= syncInterval) {
    // send a sync request to server to keep connection open
    sendSyncRequest(false);
    syncStart = currentMillis;
  }

  // more than 42 days of uptime
  if (currentMillis > 3628800000) {
    GLOBAL_STATE state;
    getGlobalState(state);
    if (state == GLOBAL_STATE::IDLE || state == GLOBAL_STATE::EMPTY_STOCK) {
      // restart when not busy
      ESP.restart();
    }
  }
}

void setup() {
  setCpuFrequencyMhz(240);
  Serial.begin(115200);

  delay(50U);

  // ---- Build config (one-shot boot log) ----
  Serial.println();
  Serial.println("========================================");
  Serial.println("@@ Weird MACHINE \n");
  Serial.printf("Machine ID:    %s\n", MACHINE_ID);
  Serial.printf("Machine size:  %d\n", MACHINE_SIZE);
  Serial.printf("Build mode:    %s\n",
#ifdef IS_DEVELOPMENT
    "DEVELOPMENT"
#else
    "PRODUCTION"
#endif
  );
  Serial.printf("Network:      %s\n",
#ifdef USE_WIFI
    "WIFI"
#else
    "GSM"
#endif
  );
  Serial.println("Modules:");
  Serial.printf("  RFID:  %s\n",
#ifdef HAS_RFID_MODULE
    "ENABLED"
#else
    "disabled"
#endif
  );
  Serial.printf("  BKASH: %s\n",
#ifdef HAS_BKASH_MODULE
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
  pinMode(NOTE_ACCEPTOR_RX, INPUT_PULLUP);
  pinMode(NOTE_ACCEPTOR_TX, OUTPUT);

  turnOffCashAcceptor();
#endif

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

  // Restore dispense-no history from NVS BEFORE MQTT connects, so the dedup
  // guard is active the moment an inflight broker message could arrive.
  loadDispenseHistory();

  setGlobalState(GLOBAL_STATE::CONNECT);
  showImage(connecting_t);
  pickBestClient();

  showImage(connecting_t);
  Serial.println("Setting up MQTT");
  setupMqtt();

  delay(50U);
  while (!establishMqttConnection()) {
    checkButtons();
  }

  sendSyncRequest(false);

  Serial.println("Now we loop.");
  showImage(pushBtnOrScanQR_t);

#if defined(HAS_RFID_MODULE)
  rdm6300.begin(GPIO_NUM_5);
#endif

  // Register the mqtt.loop() callback so runMotorSpring() can keep the
  // MQTT/SSL connection alive while the motor is spinning.
  setMqttLoopCallback([]() { mqtt.loop(); });

#if defined(HAS_RFID_MODULE)
  setRfidResetCallback([]() { lastRfidTag = 0; lastRfidTime = millis(); });
  // make internal reader debounce match our constant (ms)
  rdm6300.set_tag_timeout(RFID_DEBOUNCE_MS);
  Serial.println("RFID listener started");
#endif
}


void loop() {
  monitorHealth();

  pickBestClientInLoop();

  if (!mqtt.connected()) {
    showImage(connecting_t);

    setGlobalState(GLOBAL_STATE::CONNECT);
    Serial.println("MQTT not connected");
    while (!establishMqttConnection()) {
      checkButtons();
      if (globalData.mqttReconnectAttempts >= 3) {
        pickBestClient();
      }
      monitorHealth();
    }

  #if defined(HAS_RFID_MODULE)
    // Flush any tags the reader buffered while we were stuck reconnecting and
    // reset the debounce, without ending/beginning the reader (which leaks a
    // HardwareSerial on every call in this library).
    flushRfidReader();
  #endif
    return;
  }

  mqtt.loop();

  if (pongPending) {
    drainInbound();
    // server asked with a ping, send a pong to server
    Serial.printf("Sending pong to %s\n", pingTOPIC);
    mqtt.publish(pingTOPIC, "pong");
    mqtt.loop();
    pongPending = false;
  }

  sendCurrentStatusInLoop();

  if (!globalData.hasStock) {
    // Non-blocking empty-stock wait. Enter EMPTY_STOCK once (showImage is
    // expensive) and keep the outer loop alive so monitorHealth,
    // pickBestClientInLoop, RFID, and push buttons paths all still run.
    // Auto re-request stock every 30s so the device recovers without a 
    // reset-button press when the server restocks
    GLOBAL_STATE refillState;
    getGlobalState(refillState);
    if (refillState != GLOBAL_STATE::EMPTY_STOCK) {
      setGlobalState(GLOBAL_STATE::EMPTY_STOCK);
      showImage(refill_t);
    }

    mqtt.loop();
    checkButtons();
    sendCurrentStatusInLoop();

    static unsigned long lastRefillSyncAt = 0UL;
    if (millis() - lastRefillSyncAt > 30000U) {
      sendSyncRequest(false);
      lastRefillSyncAt = millis();
    }
  } else {
    // we got stock !

    GLOBAL_STATE currState;
    getGlobalState(currState);

    DispenseRequest currentDispReq;
    getDispenseRequest(currentDispReq);

    if (currState == GLOBAL_STATE::IDLE && !currentDispReq.isExpectingResponse) {
      showImage(pushBtnOrScanQR_t);
    }

    // check for push button (cash/group selection)
    checkButtons();

#if defined(HAS_CASH_MODULE)
    // Cash flow can block for a while and mutate both state and pending
    // request tracking. Refresh before making RFID/timeout decisions.
    getGlobalState(currState);
    getDispenseRequest(currentDispReq);
#endif

    // --- Dual-group mode: button press → AWAITING_RFID → RFID tap → dispense ---
    if (currState == GLOBAL_STATE::AWAITING_RFID) {
      if (millis() - groupSelectedAt > GROUP_RFID_TIMEOUT_MS) {
        // timed out
        Serial.println("Group selection timeout - returning to idle");
        flushRfidReader();
        selectedGroup = nullptr;
        setGlobalState(GLOBAL_STATE::IDLE);
        showImage(pushBtnOrScanQR_t);
      } else if (currentDispReq.isExpectingResponse) {
        // Awaiting server response: keep MQTT alive and watch for timeout.
        // RFID is intentionally not read here; a held card must not
        // re-trigger after the debounce expires while a request is pending
        mqtt.loop();
        getDispenseRequest(currentDispReq);

        // give up gracefully after 25s instead of rebooting.
        // the broker (cleanSession=false) still holds the queued response;
        // when it arrives later, handleDispenseResponse will run the motor
        // and the NVS-persisted dedup history prevents a double-dispense.
        if (currentDispReq.isExpectingResponse && millis() - currentDispReq.requestedAt > 25000U) {
          Serial.println("Dispense response timed out - giving up gracefully (no reboot)");
          currentDispReq.isExpectingResponse = false;
          currentDispReq.requestedAt = 0;
          setDispenseRequest(currentDispReq);
          showImage(tryAgain_t);
          delay(2000U);
          selectedGroup = nullptr;
          setGlobalState(GLOBAL_STATE::IDLE);
        }
      } 
#if defined(HAS_RFID_MODULE)
      else if (rdm6300.get_new_tag_id()) {
        uint32_t currentTag = rdm6300.get_tag_id();

        unsigned long currentTime = millis();

        // Only process if it's a different tag or enough time has passed
        if (currentTag != lastRfidTag ||
          (currentTime - lastRfidTime) > RFID_DEBOUNCE_MS) {
          
          showImage(rfidProcessing_t);
          String rfid = String(currentTag);
          Serial.print("RFID read: ");
          Serial.println(rfid);
          Serial.print("Selected group: ");
          Serial.println(selectedGroup);

          // Latch debounce state NOW, before the blocking send call,
          // so any re-read that slips through is already gated.
          lastRfidTag = currentTag;
          lastRfidTime = currentTime;

          if (!sendDispenseRequestWithGroup(rfid.c_str(), selectedGroup)) {
            recoverConnectionOrRestart("rfid dispense publish failed", RecoveryImageMode::DISPENSE_FAIL);
          }
          selectedGroup = nullptr;
        }
      }
#endif
    } else if (currState == GLOBAL_STATE::IDLE) {
      getDispenseRequest(currentDispReq);
      if (currentDispReq.isExpectingResponse) {
        // bKash server-initiated dispense - just wait
        mqtt.loop();
        if (millis() - currentDispReq.requestedAt > 15000U) {
          currentDispReq.isExpectingResponse = false;
          setDispenseRequest(currentDispReq);
          recoverConnectionOrRestart("dispense response timeout", RecoveryImageMode::DISPENSE_FAIL);
        }
      }
    }

    // Only return to IDLE if we are not in a transient state
    getGlobalState(currState);
    if (currState != GLOBAL_STATE::AWAITING_RFID &&
        currState != GLOBAL_STATE::DISPENSE &&
        currState != GLOBAL_STATE::SYNC) {
      setGlobalState(GLOBAL_STATE::IDLE);
    }
  }
}


void onMqttMessage(char* topic, byte* message, unsigned int len) {
  char msg[len + 1];
  memcpy(msg, message, len);
  msg[len] = '\0';

  Serial.println("\n MQTT message received");
  Serial.printf("Topic: %s \n", topic);
  Serial.printf("Message: %s \n", msg);

  if (strcmp(topic, pingTOPIC) == 0 && strcmp(msg, "ping") == 0) {
    pongPending = true;
    return;
  }

  StaticJsonDocument<256> mqttDoc;

  deserializeJson(mqttDoc, msg);

  String info = mqttDoc["info"];

  if (info == "dispense") {
    GLOBAL_STATE currentState;
    getGlobalState(currentState);

    int lastDispenseNo = 0;
    getLastDispenseNo(lastDispenseNo);

    Serial.printf("Last dispenseNo: %d\n", lastDispenseNo);

    int thisDispenseNo = mqttDoc["dispenseNo"].as<int>();
    Serial.printf("This dispenseNo: %d\n", thisDispenseNo);

    // proceed only if not currently dispensing, and the received dispenseNo
    // is not already in our recent history
    if (
      currentState != GLOBAL_STATE::DISPENSE &&
      currentState != GLOBAL_STATE::SYNC &&
      (lastDispenseNo == 0 || !isDispenseInHistory(thisDispenseNo))
      ) {
      handleDispenseResponse(mqttDoc);
    } else {
      Serial.println("Ignoring dispense response: already processed recently.");
    }
  } else if (info == "sync") {
    handleSyncResponse(mqttDoc);
  }
}




