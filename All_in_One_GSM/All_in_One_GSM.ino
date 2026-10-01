// remove the next comment line to switch to development mode 
#define IS_DEVELOPMENT

#define MACHINE_ID "TM_1"
#define MACHINE_SIZE 60

#define HAS_BKASH_MODULE
#define HAS_RFID_MODULE
// #define HAS_CASH_MODULE

#include <ArduinoJson.h>
#include <PinButton.h>

#include <rdm6300.h>

Rdm6300 rdm6300;

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

void checkPushButton() {
#ifdef HAS_CASH_MODULE
  pushButton.update();
  if (pushButton.isSingleClick() == 1 || pushButton.isLongClick() == 1) {
    Serial.println("Pressed push button");
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
unsigned long monitorInterval = 45000UL;
void monitorHealth() {
#if defined(IS_DEVELOPMENT)
  unsigned long currentMillis = millis();
  if (currentMillis - monitorStart >= monitorInterval) {
    Serial.printf("\n[System Monitor] Free heap at runtime: %.2f KB\n\n", ESP.getFreeHeap() / 1024.0);

    // send a sync request to server to keep connection open
    sendSyncRequest(false);

    monitorStart = currentMillis;
    if (monitorInterval < 100000) {
      monitorInterval += 1000;
    }
  }
#endif
}

void setup() {
  Serial.begin(115200);

  delay(500);

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
    delay(25U);
    digitalWrite(motorPin, LOW);
  }

  composite.init();
  graphics.init();
  showImage(welcome_t);
  delay(2000);

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

  rdm6300.begin(GPIO_NUM_5);
  rdm6300.set_tag_timeout(RFID_DEBOUNCE_MS);
  Serial.println("RFID listener started");
}


void loop() {
  monitorHealth();

  pickBestClientInLoop();

  if (mqtt.connected()) {
    mqtt.loop();
    sendCurrentStatusInLoop();

    if (!globalData.hasStock) {
      showImage(refill_t);
      while (!globalData.hasStock) {
        setGlobalState(GLOBAL_STATE::EMPTY_STOCK);
        checkResetButton();
        sendCurrentStatusInLoop();
        delay(100U);
        mqtt.loop();
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

      if (currState == GLOBAL_STATE::IDLE) {
        if (currentDispReq.isExpectingResponse) {
          // Awaiting server response: keep MQTT alive and watch for timeout
          // RFID is intentionally not read here - a held catd must not
          // re-trigger after the debounce expires while a request is pending.
          mqtt.loop();
          if (millis() - currentDispReq.requestedAt > 15000U) {
            showImage(somethingWentWrong_t);
            delay(200U);
            showImage(tryAgain_t);
            delay(1000U);

            currentDispReq.isExpectingResponse = false;
            setDispenseRequest(currentDispReq);
          }
        } else if (rdm6300.get_new_tag_id()) {
          uint32_t currentTag = rdm6300.get_tag_id();

          unsigned long currentTime = millis();

          // Only process if it's a different tag or enough time has passed
          if (currentTag != lastRfidTag ||
            (currentTime - lastRfidTime) > RFID_DEBOUNCE_MS) {

            showImage(rfidProcessing_t);
            String rfid = String(currentTag);
            Serial.print("RFID read: ");
            Serial.println(rfid);

            // Latch debounce state NOW, before blocking send call
            // So any re-read that slips through is already gated
            lastRfidTag = currentTag;
            lastRfidTime = currentTime;

            setGlobalState(GLOBAL_STATE::DISPENSE);
            sendDispenseRequest(rfid.c_str());
          }
        }
      }
    }
    setGlobalState(GLOBAL_STATE::IDLE);
  } else {
    showImage(connecting_t);

    setGlobalState(GLOBAL_STATE::CONNECT);
    Serial.println("MQTT not connected");
    while (!establishMqttConnection()) {
      checkResetButton();
      if (globalData.mqttReconnectAttempts >= 3) {
        pickBestClient();
      }
    }
    // RFID hardware keeps reading while we were reconnecting. Restart the
    // reader to flush any buffered tags and reset the debounce so a card
    // that was held during the outage cannot fire a spurious dispense
    rdm6300.end();
    delay(100U);
    rdm6300.begin(GPIO_NUM_5);
    lastRfidTag = 0;
    lastRfidTime = mills();
  }
}


void onMqttMessage(char* topic, byte* message, unsigned int len) {
  char msg[len + 1];
  memcpy(msg, message, len);
  msg[len] = '\0';

  Serial.println("\n MQTT message recieved");
  Serial.printf("Topic: %s \n", topic);
  Serial.printf("Message: %s \n", msg);

  if (strcmp(topic, pingTOPIC) == 0 && strcmp(msg, "ping") == 0) {
    Serial.printf("Sending pong to %s\n", pingTOPIC);

    unsigned long start = millis();
    while (millis() - start < 1 * 60000UL) {
      GLOBAL_STATE currentState;
      getGlobalState(currentState);
      if (currentState != GLOBAL_STATE::DISPENSE) break;
      delay(50U);
    }
    mqtt.publish(pingTOPIC, "pong");
    mqtt.loop();
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

    Serial.print("Last dispenseNo: ");
    Serial.println(lastDispenseNo);

    int thisDispenseNo = mqttDoc["dispenseNo"].as<int>();
    Serial.print("This dispenseNo: ");
    Serial.println(thisDispenseNo);

    // proceed only if not currently dispensing, and the received dispenseNo
    // is not already in out recent history
    if (
      currentState != GLOBAL_STATE::DISPENSE &&
      (lastDispenseNo == 0 || !isDispenseInHistory(thisDispenseNo))
      ) {
      handleDispenseResponse(mqttDoc);
    }
  } else if (info == "sync") {
    handleSyncResponse(mqttDoc);
  }
}




