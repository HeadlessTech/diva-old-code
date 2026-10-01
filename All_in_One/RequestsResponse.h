// RequestsResponse.h
#pragma once

#include "Global.h"
#include "MQTT.h"
#include "Dispense.h"
#include "display.h"

// Callback registered by the main sketch to reset the RFID debounce state
// (lastRfidTag / lastRfidTime) whenever the RFID reader is restarted.
// This prevents a held card from instantly re-triggering after a dispense.
static void (*_rfidResetCallback)() = nullptr;
inline void setRfidResetCallback(void (*fn)()) { _rfidResetCallback = fn; }

#if defined(HAS_RFID_MODULE)
// C-belt: flush any RFID tag events the reader queued while we were busy
// (dispensing / reconnecting), WITHOUT tearing down the HardwareSerial.
// Uses the library's own read path (no Serial1 buffer-sharing assumptions)
// and avoids the rdm6300 end()/begin() pair that leaks ~150 bytes per call.
// The 5 s sketch debounce (lastRfidTag / lastRfidTime) is the real gate that
// prevents a held / rapidly re-tapped card from re-firing a dispense.
void flushRfidReader() {
  for (uint8_t i = 0; i < 4; i++) { rdm6300.get_new_tag_id(); delay(5U); }
  if (_rfidResetCallback) _rfidResetCallback();
}
#endif

bool sendSyncRequest(bool isAcknowledgement) {
  globalData.isServerCommunication = false;

  StaticJsonDocument<48> mqttDocToSend;

  Serial.println("Sending sync request...");
  
  if (!establishMqttConnection(RecoveryImageMode::NONE)) {
    return false;
  }

  int lastDispenseNo = 0;
  getLastDispenseNo(lastDispenseNo);
  // Only skip if this is an acknowledgement send and there is no
  // dispense number to acknowledge. A plain sync (isAcknowledgement=false)
  // must always be published so the machine gets the initial stock count.
  if (isAcknowledgement && lastDispenseNo == 0) return true;

  mqttDocToSend["info"] = "sync";
  if (isAcknowledgement) mqttDocToSend["dispenseNo"] = lastDispenseNo;

  mqttDocToSend.shrinkToFit();

  char mqtt_buffer[48];

  serializeJson(mqttDocToSend, mqtt_buffer);

  if (!publishBuffer(mqtt_buffer, RecoveryImageMode::CONNECT_FAIL)) {
    return false;
  }

  Serial.print("Published: ");
  Serial.println(mqtt_buffer);
  return true;
}

bool sendDispenseRequest(const char* rfid) {
  // Run the MQTT loop briefly instead of blocking — keeps the SSL connection
  // alive and avoids the 1 s dead-window that triggered reconnects.
  mqtt.loop();

  StaticJsonDocument<48> mqttDocToSend;

  if (rfid != nullptr) mqttDocToSend["rfid"] = rfid;
  mqttDocToSend["info"] = "dispense";

  char mqtt_buffer[48];

  serializeJson(mqttDocToSend, mqtt_buffer);

  if (!publishBuffer(mqtt_buffer, RecoveryImageMode::DISPENSE_FAIL)) {
    return false;
  }

  Serial.print("Published: ");
  Serial.println(mqtt_buffer);

  DispenseRequest currentDispReq;
  currentDispReq.isExpectingResponse = true;
  currentDispReq.requestedAt = millis();
  setDispenseRequest(currentDispReq);
  return true;
}


void handleDispenseResponse(JsonDocument mqttDoc) {
  int status = mqttDoc["status"];

  Serial.println(status);

  switch (status) {
    case 200:
      {
        int newDispenseNo = mqttDoc["dispenseNo"].as<int>();
        setLastDispenseNo(newDispenseNo);
        String type = mqttDoc["type"];

        if (type == "bkash") {
#if defined(HAS_BKASH_MODULE)
          showImage(bkashComplete_t);
#endif
          delay(1000U);
        } else {
          // was I expecting a dispense ?
          DispenseRequest currentDispReq;
          getDispenseRequest(currentDispReq);
          if (currentDispReq.isExpectingResponse) {
            Serial.println("Removing expectations...");
            currentDispReq.isExpectingResponse = false;
            currentDispReq.requestedAt = 0;
            setDispenseRequest(currentDispReq);
          }
        }

        Serial.println(newDispenseNo);

        setGlobalState(GLOBAL_STATE::DISPENSE);
        // show image
        showImage(dispensing_t);
        delay(100U);

        // check if multiple columns are specified
        if (mqttDoc.containsKey("columns")) {
          JsonArray columns = mqttDoc["columns"];
          Serial.print("Dispensing multiple products: ");
          Serial.println(columns.size());
          
          for (int i = 0; i < columns.size(); i++) {
            int col = columns[i];
            Serial.print("Dispensing from column: ");
            Serial.println(col);
            
            // set column for motor selection
            dispenseProcess(col);
            mqtt.loop();

            // Brief inter-dispense pause — keep MQTT alive throughout.
            for (int j = 0; j < 2; j++) { mqtt.loop(); delay(100U); }
          }
        } else {
          // single column dispense (existing logic)
          int column = mqttDoc["column"];
          dispenseProcess(column);
          mqtt.loop();
        }
        showImage(collectProduct_t);
        setGlobalState(GLOBAL_STATE::SYNC);
        sendSyncRequest(true);

#if defined(HAS_RFID_MODULE)
        // Flush any tags the reader buffered during the dispense (the motor run
        // can take a few seconds, during which the library wasn't being polled),
        // then reset the 5 s debounce so a card held throughout can't re-fire.
        // We no longer end()/begin() the reader - that leaks a HardwareSerial.
        flushRfidReader();
#endif

        setGlobalState(GLOBAL_STATE::IDLE);
        for (uint8_t i = 0; i < 8; i++) { mqtt.loop(); delay(250U); }
        break;
      }
    case 400:
      Serial.println("UnAuthorized request");
      showImage(unauthorizedRequest_t);
      delay(3000U);
      break;
    case 402:
      Serial.println("RFID BLOCKED");
#if defined(HAS_RFID_MODULE)
      showImage(rfidBlocked_t);
#else
      showImage(somethingWentWrong_t);
#endif
      delay(3000U);
      break;
    case 403:
      Serial.println("Limit crossed");
#if defined(HAS_RFID_MODULE)
      showImage(rfidLimitCrossed_t);
#else
      showImage(somethingWentWrong_t);
#endif
      delay(3000U);
      break;
    case 404:
      Serial.println("Product not found");
      showImage(refill_t);
      delay(3000U);
      break;
    default:
      Serial.println("invalid status code");
      showImage(somethingWentWrong_t);
      delay(1500U);
      showImage(tryAgain_t);
      delay(1500U);
      break;
  }

  if (status != 200) {
    if (!globalData.hasStock) {
      showImage(refill_t);
      setGlobalState(GLOBAL_STATE::EMPTY_STOCK);
      Serial.println("product sesh.");
    }

    // was I expecting a dispense ?
    DispenseRequest currentDispReq;
    getDispenseRequest(currentDispReq);
    if (currentDispReq.isExpectingResponse) {
      Serial.println("Removing expectations...");
      currentDispReq.isExpectingResponse = false;
      currentDispReq.requestedAt = 0;
      setDispenseRequest(currentDispReq);
    }
  }
}

void handleSyncResponse(const JsonDocument& mqttDoc) {
  int status = mqttDoc["status"];

  switch (status) {
    case 200:
      {
        globalData.isServerCommunication = true;

        int count = mqttDoc["count"].as<int>();
        globalData.hasStock = count >= 1;

        if (count <= 0) {
          setGlobalState(GLOBAL_STATE::EMPTY_STOCK);
          Serial.println("product sesh.");
        } else {
          setGlobalState(GLOBAL_STATE::IDLE);

#if defined(HAS_RFID_MODULE)
          flushRfidReader();
#endif

        }
        break;
      }
    default:
      Serial.println("Something went wrong!");
      break;
  }
}

