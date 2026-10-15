#pragma once

#define CASH_PRICE 30
#define NOTE_ACCEPTOR_TX 14
#define NOTE_ACCEPTOR_RX 35

// Retains partial credit until a cash dispense completes.
int cashCredit = 0;

void turnOffCashAcceptor() {
  digitalWrite(NOTE_ACCEPTOR_TX, LOW);
}

void turnOnCashAcceptor() {
  if (!globalData.hasStock) return;
  showImage(insertTenTaka_t);
  loadOfflineStock();
  if (!globalData.hasStock) {
    showOfflineStatus();
    return;
  }
  setGlobalState(GLOBAL_STATE::CASH);
  digitalWrite(NOTE_ACCEPTOR_TX, HIGH);

  Serial.println("Accepting note");

  const unsigned long timeoutMs = 60000UL;
  unsigned long startTime = millis();

  while (true) {
    unsigned long pwm = pulseIn(NOTE_ACCEPTOR_RX, LOW);
    discardRfidReads();
    if (pwm != 0) {
      if (cashCredit == 0) {
        showImage(noteProcessing_t);
      }
      cashCredit++;
      // Serial.print(cashCredit);
      // Serial.print(",");
      startTime = millis(); // Reset timeout on activity
    }

    if (cashCredit >= CASH_PRICE - 5 && cashCredit <= CASH_PRICE + 5) {
      setGlobalState(GLOBAL_STATE::DISPENSE);
      turnOffCashAcceptor();
      showImage(noteAccepted_t);
      Serial.println(" ");
      Serial.print(cashCredit);
      Serial.println("$ Credited! Acceptor Disabled");
      cashCredit = 0;
      dispenseOffline();
      break;
    }

    if (millis() - startTime >= timeoutMs) {
      turnOffCashAcceptor();
      Serial.println("Note acceptor timeout.");
      showImage(timeOut_t);
      waitOffline(2000U);
      break;
    }
  }
  flushRfidReader();
  showOfflineStatus();
}
