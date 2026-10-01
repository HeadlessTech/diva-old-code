// GsmClient.h
#pragma once

#define TINY_GSM_MODEM_SIM800
#include <TinyGsmClient.h>

#define GSM_APN "INTERNET"
#define GPRS_USER ""
#define GPRS_PASS ""

TinyGsm modem(Serial2);
TinyGsmClient gsmClient(modem);

bool triedOnce = false;
bool tryGsmConnect() {
  Serial.println("Trying to connect gsm...");
  
  Serial2.begin(115200);
  delay(500);

  // if (triedOnce) {
    if (!modem.restart()) {          // full modem reset, clears stale PDP
      Serial.println("[GSM] Modem restart failed");
      return false;
    }
  // }

  triedOnce = true;

  unsigned long start = millis();
  while (!modem.waitForNetwork(1000) && millis() - start < 30000U) {
    delay(10);
  }
  if (!modem.isNetworkConnected()) {
    static constexpr char* msg = "[GSM] !modem.isNetworkConnected()";
    Serial.println(msg);
    return false;
  }

  if (!modem.gprsConnect(GSM_APN, GPRS_USER, GPRS_PASS)) {
    Serial.println("[GSM] !modem.gprsConnect(...)");
    return false;
  }

  return true;
}
