// WifiClient.h
#pragma once

#include <WiFi.h>

#define WIFI_SSID "Diva"
#define WIFI_PASS "headtechless"

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
  Serial.print("PASS: ");
  Serial.println(WIFI_PASS);

  WiFi.disconnect(true);
  delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000U) {
    delay(100U);
  }

  return WiFi.status() == WL_CONNECTED;
}
