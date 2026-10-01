// MQTT.h
#pragma once

#include <PubSubClient.h> 

#include "Global.h"
#include "SSL.h"
#include "Dispense.h"

#define MQTT_BROKER "a35i41v9e1l1gt-ats.iot.ap-south-1.amazonaws.com"
#define MQTT_PORT 8883  // ports usually this while using SSL
#define MQTT_SUB_QOS 1
#define MQTT_PACKET_SIZE 512

// Last Will and Testament
#define MQTT_LWT_QOS 1
#define MQTT_LWT_RETAIN false

#ifdef IS_DEVELOPMENT
#define pubTOPIC "vms/" MACHINE_ID "/pub/dev"
#define subTOPIC "vms/" MACHINE_ID "/sub/dev"
#define pingTOPIC "vms/" MACHINE_ID "/ping/dev"
#define lwtTOPIC "vms/" MACHINE_ID "/status/dev"
#else
#define pubTOPIC "vms/" MACHINE_ID "/pub"
#define subTOPIC "vms/" MACHINE_ID "/sub"
#define pingTOPIC "vms/" MACHINE_ID "/ping"
#define lwtTOPIC "vms/" MACHINE_ID "/status"
#endif

PubSubClient mqtt;

void onMqttMessage(char* topic, byte* message, unsigned int len);

void setupMqtt() {
  mqtt.setClient(*gsmSSLClient);

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setBufferSize(MQTT_PACKET_SIZE);
  mqtt.setCallback(onMqttMessage);
  mqtt.setKeepAlive(86400); // Send a PINGREQ every N second(s) to keep the connection alive and detect drops.
};

void sendCurrentStatus() {
  StaticJsonDocument<48> doc;
  doc["message"] = "online";

  char mqttBuffer[48];
  serializeJson(doc, mqttBuffer);

  Serial.println(mqttBuffer);

  mqtt.publish(lwtTOPIC, mqttBuffer, MQTT_LWT_RETAIN);
  mqtt.loop();
  // mqtt.loop(); // Ensure the "online" message is sent immediately
  delay(10U);
  Serial.print("Published status on topic: ");
  Serial.println(lwtTOPIC);
}


#define WIFI_MQTT_TIMEOUT_MS 15000UL
#define GSM_MQTT_TIMEOUT_MS  40000UL

bool connectMqtt() {
  const unsigned long timeoutMs = WIFI_MQTT_TIMEOUT_MS;

  Serial.println("MQTT Connecting ....");
  int start = millis();
  while (!mqtt.connected()) {
    if (millis() - start > timeoutMs) {  // timeout
      Serial.printf("MQTT connect timeout (%lu ms)\n", timeoutMs);
      return false;
    }

    static const char* lwtPayload = "{\"message\":\"offline\"}";
    bool mqttConnectResponse = mqtt.connect(MACHINE_ID, lwtTOPIC, MQTT_LWT_QOS, MQTT_LWT_RETAIN, lwtPayload);
    Serial.println("CONNECTION ESTABLISHED WITH LWT");
    if (mqttConnectResponse) {
      // Successfully connected, publish the "online" status
      sendCurrentStatus();
      break;
    } else {
      delay(500U);
    }
  }

  Serial.println("MQTT broker Connected!");
  return true;
}

void subscribeToMqttTopic() {
  Serial.println("MQTT Subscribing....");
  mqtt.subscribe(subTOPIC, MQTT_SUB_QOS);
  mqtt.subscribe(pingTOPIC, MQTT_SUB_QOS);
  mqtt.loop();
  Serial.println("subscribed on topic: " subTOPIC);
  Serial.println("subscribed on topic: " pingTOPIC);
}


#define BASE_RETRY_MS 2000UL 
#define MAX_RETRY_MS  60000UL

bool establishMqttConnection() {
  if (mqtt.connected()) return true;
  // showImage(connecting_t);

  uint8_t cappedAttempts = min(globalData.mqttReconnectAttempts, 5);
  unsigned long delayMs = min(BASE_RETRY_MS << cappedAttempts, MAX_RETRY_MS);

  if (globalData.mqttReconnectAttempts) {
    Serial.printf("MQTT retry %u in %lu ms\n", globalData.mqttReconnectAttempts, delayMs);
    delay(delayMs);
  }

  if (globalData.mqttReconnectAttempts >= 4) {
    ESP.restart();
  }

  if (connectMqtt()) {
    globalData.mqttReconnectAttempts = 0;
    subscribeToMqttTopic();
    delay(250U);
    return true;
  }
  globalData.latestConnectionTrial = millis();
  globalData.mqttReconnectAttempts++;
  Serial.println("Connection failed!");
  // showImage(connectionFailed_t);
  delay(3000U);
  return false;
}

void publishBuffer(const char* buffer) {
  mqtt.publish(pubTOPIC, buffer);
  mqtt.loop();
}

unsigned long lastStatusSentAt = 0UL;
void sendCurrentStatusInLoop() {
  if (millis() - lastStatusSentAt > 90000U) {   // every minute and a half
    sendCurrentStatus();
    lastStatusSentAt = millis();
  }
}
