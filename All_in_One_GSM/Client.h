// Client.h
#pragma once

#include "SSL.h"
#include "MQTT.h"
#include "RequestsResponse.h"

// picks client with the best rssi value
void pickBestClient() {
  bool isGsmConnected = tryGsmConnect();

  if (!isGsmConnected) {
    Serial.println("\n/////////////////////////////////");
    Serial.println("Could not connect gsm.");
    Serial.println("Restarting .....");
    Serial.println("/////////////////////////////////\n");
    delay(3000U);
    ESP.restart();
    return;
  }

  int gsmRssiRaw = modem.getSignalQuality();   // 0-31 or 99
  int gsmRssi = (gsmRssiRaw == 99) ? -120 : -113 + 2 * gsmRssiRaw;
  globalData.strength = gsmRssi;
  delay(50U);

  Serial.print("Using GSM Strength: ");
  Serial.println(gsmRssi);

  initializeSSL();
}


constexpr int RSSI_THRESHOLD = -75;
unsigned long lastRssiCheck = 0;

void pickBestClientInLoop() {
  // if (millis() - lastRssiCheck < 30000UL) return;
  // lastRssiCheck = millis();

  // int rssi = -120;
  // if (globalData.useWifi) {
  //   rssi = WiFi.RSSI();
  // } else {
  //   int gsmRssiRaw = modem.getSignalQuality();
  //   rssi = (gsmRssiRaw == 99) ? -120 : -113 + 2 * gsmRssiRaw;
  // }

  // Serial.print("Current RSSI: ");
  // Serial.println(rssi);
  // globalData.strength = rssi;

  // if (rssi > RSSI_THRESHOLD) return; // good enough signal strength

  // int currentRssi = globalData.strength;

  // // re-select
  // setGlobalState(GLOBAL_STATE::CONNECT);
  // pickBestClient();
  // sendSyncRequest(false);
}