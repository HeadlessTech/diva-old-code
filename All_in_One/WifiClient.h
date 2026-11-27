// WifiClient.h
#pragma once

#include <WiFi.h>

// WIFI_SSID and WIFI_PASSWORD come from config.h.

WiFiClient wifiClient;

bool tryWifiConnect() {
  static bool alreadySet = false;
  if (!alreadySet) {
    WiFi.setSleep(false);
    alreadySet = true;
  }
  
  Serial.println("Trying to connect wifi...");

  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);

  WiFi.disconnect(true);
  delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000U) {
    delay(100U);
  }

  return WiFi.status() == WL_CONNECTED;
}
