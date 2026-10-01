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

  while (true) {
    int pwm = pulseIn(NOTE_ACCEPTOR_RX, LOW);
    if (pwm != 0) {
      if (credit == 0) {
        showImage(noteProcessing_t);
      }
      
      credit++;
      Serial.print(credit);
      Serial.print(",");
      startTime = millis(); // Reset timeout on activity
    }

    if (credit >= CASH_PRICE - 5 && credit <= CASH_PRICE + 5) {
      setGlobalState(GLOBAL_STATE::DISPENSE);
      showImage(noteAccepted_t);
      Serial.println(" ");
      Serial.print(credit);
      Serial.println("$ Credited! Acceptor Disabled");
      turnOffCashAcceptor();
      sendDispenseRequest(nullptr);
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
  // need to check if network good
  
  // sendSyncRequest(false);

  Serial.println(globalData.isServerCommunication ? "YES" : "NOO");

  bool isNetworkOk = false;

  unsigned long start = millis();
  while (true) {
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
      // showImage(syncFailed_t);
      delay(1000U);
      showImage(tryAgain_t);
      delay(1000U);

      break;
    }
  }

  Serial.println(isNetworkOk ? "NETWORK OK" : "NET NOT OK");

  return isNetworkOk;
}

