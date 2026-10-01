// comment this line on production 
// #define IS_DEVELOPMENT

// comment this line when using GSM
// #define USE_WIFI

#define MACHINE_ID "BAIO20"
#define MACHINE_SIZE 20

#define HAS_BKASH_MODULE
#define HAS_RFID_MODULE
#define HAS_CASH_MODULE

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

PinButton pushButton(GPIO_NUM_39);
PinButton resetButton(GPIO_NUM_36);

uint32_t lastRfidTag = 0;
unsigned long lastRfidTime = 0;
// 5 s cooldown: long enough to survive a motor run + sync round-trip so the
// card cannot re-trigger immediately after a dispense or after reconnect.
const unsigned long RFID_DEBOUNCE_MS = 5000;

static bool pongPending = false; // toggled by onMQTT handler

void checkPushButton() {
#ifdef HAS_CASH_MODULE
  pushButton.update();
  if (pushButton.isSingleClick() == 1 || pushButton.isLongClick() == 1) {
    Serial.println("Pressed push button");

    // Guard: only start cash flow when idle and not waiting for another response
    GLOBAL_STATE currBtnState;
    getGlobalState(currBtnState);
    if (currBtnState != GLOBAL_STATE::IDLE) return;

    DispenseRequest currBtnDispReq;
    getDispenseRequest(currBtnDispReq);
    if (currBtnDispReq.isExpectingResponse) return;

    bool isOk = checkServerCommunicationForCashAcceptor();
    if (isOk) {
      turnOnCashAcceptor();
    }
  }
#endif
}


void checkResetButton() {
  resetButton.update();
  if (resetButton.isSingleClick() == 1) {
    Serial.println("Pressed reset button");
    sendSyncRequest(false);
  }
  if (resetButton.isLongClick() == 1) {
    showImage(connecting_t);
    Serial.println("Long pressed reset button");
    pickBestClient();
    setupMqtt();
    sendSyncRequest(false);
  }
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
    checkResetButton();
  }

  sendSyncRequest(false);

  Serial.println("Now we loop.");
  showModuleImage();

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

  if (mqtt.connected()) {
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
      // pickBestClientInLoop, RFID, and push-button paths all still run.
      // Auto-re-request stock every 30 s so the device recovers without a
      // reset-button press when the server restocks.
      GLOBAL_STATE refillState;
      getGlobalState(refillState);
      if (refillState != GLOBAL_STATE::EMPTY_STOCK) {
        setGlobalState(GLOBAL_STATE::EMPTY_STOCK);
        showImage(refill_t);
      }

      mqtt.loop();
      checkResetButton();
      sendCurrentStatusInLoop();

      static unsigned long lastRefillSyncAt = 0UL;
      if (millis() - lastRefillSyncAt > 30000U) {
        sendSyncRequest(false);
        lastRefillSyncAt = millis();
      }
    } else {
      GLOBAL_STATE currState;
      getGlobalState(currState);

      DispenseRequest currentDispReq;
      getDispenseRequest(currentDispReq);

      if (currState == GLOBAL_STATE::IDLE && !currentDispReq.isExpectingResponse) {
        showModuleImage();
      }

      // check for push button (cash)
      checkPushButton();

      // Cash flow can block for a while and mutate both state and pending
      // request tracking. Refresh before making RFID/timeout decisions.
      getGlobalState(currState);
      getDispenseRequest(currentDispReq);

      if (currState == GLOBAL_STATE::IDLE) {
        if (currentDispReq.isExpectingResponse) {
          // Awaiting server response: keep MQTT alive and watch for timeout.
          // RFID is intentionally not read here — a held card must not
          // re-trigger after the debounce expires while a request is pending.
          mqtt.loop();
          getDispenseRequest(currentDispReq);
          // give up gracefully after 25 s instead of rebooting.
          // The broker (cleanSession=false) still holds the queued response;
          // when it arrives later, handleDispenseResponse will run the motor
          // and the NVS-persisted dedup history prevents a double-dispense.
          if (currentDispReq.isExpectingResponse && millis() - currentDispReq.requestedAt > 25000U) {
            Serial.println("Dispense response timeout - giving up gracefully (no reboot)");
            currentDispReq.isExpectingResponse = false;
            currentDispReq.requestedAt = 0;
            setDispenseRequest(currentDispReq);
            showImage(tryAgain_t);
            delay(2000U);
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

            // Latch debounce state NOW, before the blocking send call,
            // so any re-read that slips through is already gated.
            lastRfidTag = currentTag;
            lastRfidTime = currentTime;

            setGlobalState(GLOBAL_STATE::DISPENSE);
            if (!sendDispenseRequest(rfid.c_str())) {
              recoverConnectionOrRestart("rfid dispense publish failed", RecoveryImageMode::DISPENSE_FAIL);
            }
          }
        }
#endif
      }
      // Reset state to IDLE only when we actually ran the dispense/RFID/cash
      // branch. Do NOT reset in the empty-stock branch - keeping EMPTY_STOCK
      // stable across iterations lets the "enter once" guard above avoid
      // re-flipping the framebuffer every loop tick.
      setGlobalState(GLOBAL_STATE::IDLE);
    }
  } else {
    showImage(connecting_t);

    setGlobalState(GLOBAL_STATE::CONNECT);
    Serial.println("MQTT not connected");
    while (!establishMqttConnection()) {
      checkResetButton();
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

  StaticJsonDocument<192> mqttDoc;

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




