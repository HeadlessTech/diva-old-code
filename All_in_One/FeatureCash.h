#pragma once

#define CASH_PRICE 30
#define NOTE_ACCEPTOR_TX 14
#define NOTE_ACCEPTOR_RX 35

void turnOffCashAcceptor() {
  digitalWrite(NOTE_ACCEPTOR_TX, LOW);
}

void turnOnCashAcceptor() {
  digitalWrite(NOTE_ACCEPTOR_TX, HIGH);
  showImage(insertTenTaka_t);

  Serial.println("Accepting note");

  const unsigned long timeoutMs = 50 * 1000; // n seconds
  unsigned long startTime = millis();

  int credit = 0;
  unsigned long lastMqttLoop = millis();

  while (true) {
    if (!isTransportConnected()) {
      Serial.println("Network lost while accepting cash.");
      turnOffCashAcceptor();
      recoverConnectionOrRestart("network lost during cash acceptance", RecoveryImageMode::CASH_FAIL);
      break;
    }

    int pwm = pulseIn(NOTE_ACCEPTOR_RX, LOW, 200000); // 200ms timeout so loop stays responsive
    if (pwm != 0) {
      if (credit == 0) {
        showImage(noteProcessing_t);
      }
      
      credit++;
      Serial.print(credit);
      Serial.print(",");
      startTime = millis(); // Reset timeout on activity
    }

    // Keep MQTT alive during cash acceptance window
    if (millis() - lastMqttLoop >= 500U) {
      mqtt.loop();
      lastMqttLoop = millis();
    }

    if (credit >= CASH_PRICE - 5) {
      setGlobalState(GLOBAL_STATE::DISPENSE);
      showImage(noteAccepted_t);
      Serial.println(" ");
      Serial.print(credit);
      Serial.println("$ Credited! Acceptor Disabled");
      turnOffCashAcceptor();
      if (!sendDispenseRequest(nullptr)) {
        recoverConnectionOrRestart("cash dispense publish failed", RecoveryImageMode::DISPENSE_FAIL);
        break;
      }
      delay(2500U);
      break;
    }

    if (millis() - startTime > timeoutMs) {
      Serial.println("Note acceptor timeout.");
      turnOffCashAcceptor();
      break;
    }
  }
}

bool checkServerCommunicationForCashAcceptor() {
  // Fresh sync to verify server communication before accepting cash
  showImage(connecting_t);

  if (!sendSyncRequest(false)) {
    Serial.println("Cash preflight sync failed.");
    recoverConnectionOrRestart("cash preflight sync failed", RecoveryImageMode::CASH_FAIL);
    return false;
  }

  Serial.println(globalData.isServerCommunication ? "YES" : "NOO");

  bool isNetworkOk = false;

  unsigned long start = millis();
  while (true) {
    if (!isTransportConnected()) {
      Serial.println("Network lost during cash preflight.");
      recoverConnectionOrRestart("cash preflight transport lost", RecoveryImageMode::CASH_FAIL);
      break;
    }

    mqtt.loop();  // process incoming MQTT messages so handleSyncResponse() can fire

    if (globalData.isServerCommunication) {
      if (!globalData.hasStock) {
        Serial.println("product sesh.");
        showImage(refill_t);
        break;
      }
      isNetworkOk = true;
      break;
    }

    if (millis() - start > 15000) {  // timeout
      Serial.println("TIMED OUT");
      recoverConnectionOrRestart("cash preflight sync timeout", RecoveryImageMode::CASH_FAIL);
      break;
    }
  }

  Serial.println(isNetworkOk ? "NETWORK OK" : "NET NOT OK");

  return isNetworkOk;
}

