// Client.h
#pragma once

#include "SSL.h"
#include "MQTT.h"
#include "RequestsResponse.h"

// picks client with the best rssi value
void pickBestClient() {
#ifdef USE_WIFI
  bool isWifiConnected = tryWifiConnect();

  if (!isWifiConnected) {
    showConnectingRetrySequence();
    Serial.println("\n/////////////////////////////////");
    Serial.println("Could not connect to wifi.");
    Serial.println("Restarting .....");
    Serial.println("/////////////////////////////////\n");
    delay(CONNECTING_BEFORE_RESTART_MS);
    ESP.restart();
    return;
  }

  int wifiRssi = isWifiConnected ? WiFi.RSSI() : -120;
  globalData.strength = wifiRssi;
  delay(50U);

  Serial.print("Using WiFi Strength: ");
  Serial.println(wifiRssi);
#else
  bool isGsmConnected = tryGsmConnect();

  if (!isGsmConnected) {
    showConnectingRetrySequence();
    Serial.println("\n/////////////////////////////////");
    Serial.println("Could not connect to GSM.");
    Serial.println("Restarting .....");
    Serial.println("/////////////////////////////////\n");
    delay(CONNECTING_BEFORE_RESTART_MS);
    ESP.restart();
    return;
  }

  int gsmRssi = modem.getSignalQuality();
  globalData.strength = gsmRssi;
  delay(50U);

  Serial.print("Using GSM Signal Quality: ");
  Serial.println(gsmRssi);
#endif

  initializeSSL();
}


constexpr int RSSI_THRESHOLD = -75;

// Called every loop iteration to detect a dead transport quickly and recover
// before a cash or RFID flow starts on a stale broker session.
void pickBestClientInLoop() {
  static unsigned long lastTransportCheckAt = 0UL;

  if (millis() - lastTransportCheckAt < 2000U) return;
  lastTransportCheckAt = millis();

  if (isTransportConnected()) return;

#ifdef USE_WIFI
  Serial.println("WiFi transport lost in main loop.");
#else
  Serial.println("GSM transport lost in main loop.");
#endif
  recoverConnectionOrRestart("transport lost in main loop", RecoveryImageMode::CONNECT_FAIL);
}