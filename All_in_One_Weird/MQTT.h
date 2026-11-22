// MQTT.h
#pragma once

#include <PubSubClient.h> 

#include "Global.h"
#include "SSL.h"
#include "Dispense.h"
#include "display.h"

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

#ifdef USE_WIFI
#define SSL_CLIENT_TO_USE wifiSSLClient
#else
#define SSL_CLIENT_TO_USE gsmSSLClient
#endif

PubSubClient mqtt;

void onMqttMessage(char* topic, byte* message, unsigned int len);

enum class RecoveryImageMode : uint8_t {
  CONNECT_FAIL,
  CASH_FAIL,
  DISPENSE_FAIL,
  NONE
};

bool establishMqttConnection(RecoveryImageMode imageModeOnFailure = RecoveryImageMode::CONNECT_FAIL);

constexpr unsigned long TRY_AGAIN_IMAGE_MS = 2000UL;
constexpr unsigned long SOMETHING_WRONG_IMAGE_MS = 600UL;
constexpr unsigned long CONNECTING_BEFORE_RESTART_MS = 400UL;

bool isTransportConnected() {
#ifdef USE_WIFI
  return WiFi.status() == WL_CONNECTED;
#else
  return modem.isNetworkConnected();
#endif
}

void resetMqttTransport(const char* reason) {
  Serial.print("Resetting MQTT transport: ");
  Serial.println(reason);

  globalData.isServerCommunication = false;

  DispenseRequest clearedRequest;
  setDispenseRequest(clearedRequest);
}

void showRecoveryImageMode(RecoveryImageMode imageMode) {
  if (imageMode == RecoveryImageMode::NONE) return;

  if (imageMode == RecoveryImageMode::DISPENSE_FAIL) {
    showImage(somethingWentWrong_t);
    delay(SOMETHING_WRONG_IMAGE_MS);
  }

  showImage(tryAgain_t);
  delay(TRY_AGAIN_IMAGE_MS);
  showImage(connecting_t);
}

void showConnectingRetrySequence() {
  showRecoveryImageMode(RecoveryImageMode::CONNECT_FAIL);
}

bool recoverConnectionOrRestart(const char* reason, RecoveryImageMode imageMode = RecoveryImageMode::CONNECT_FAIL) {
  Serial.print("Recovering connection after failure: ");
  Serial.println(reason);

  // Broken TLS sessions have been aborting inside SSLClient during in-place
  // recovery. Hard reboot is safer here and matches desired machine behavior.
  globalData.isServerCommunication = false;

  DispenseRequest clearedRequest;
  setDispenseRequest(clearedRequest);

  showRecoveryImageMode(imageMode);

  Serial.println("Restarting ESP to clear broken TLS state...");
  delay(CONNECTING_BEFORE_RESTART_MS);
  ESP.restart();
  return false;
}

void setupMqtt() {
  mqtt.setClient(*SSL_CLIENT_TO_USE);

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setBufferSize(MQTT_PACKET_SIZE);
  mqtt.setCallback(onMqttMessage);
  mqtt.setKeepAlive(30); // Detect stale broker sessions quickly after AP/network failures.
};

// Flush pending inbound so SSL has nothing to discard
void drainInbound(uint8_t passes = 3) {
  for (uint8_t i = 0; i < passes; i++) {
    mqtt.loop();
    delay(5U);
  } 
}

// Build identifier sent with the "online" status so the server can tell which
// firmware build each machine runs. The server ignores unknown fields.
#ifndef FW_VERSION
#define FW_VERSION "AIO_W " __DATE__ " " __TIME__
#endif

// The status JSON is built with snprintf into a fixed stack buffer (no
// ArduinoJson document, to keep RAM low). Enforce at compile time that it
// always fits and is valid JSON: a truncated or unescaped payload would fail
// JSON.parse on the server and the machine would never show as online.
#define STATUS_BUFFER_SIZE 64
constexpr bool fwIsJsonSafe(const char* s) {
  return *s == '\0' ? true
       : (*s != '"' && *s != '\\' && (unsigned char)*s >= 0x20 && fwIsJsonSafe(s + 1));
}
static_assert(sizeof("{\"message\":\"online\",\"fw\":\"\"}") - 1 + sizeof(FW_VERSION) - 1 < STATUS_BUFFER_SIZE,
              "FW_VERSION is too long for the status buffer");
static_assert(fwIsJsonSafe(FW_VERSION), "FW_VERSION must not contain quotes, backslashes or control characters");

bool sendCurrentStatus() {
  // e.g. {"message":"online","fw":"AIO_W Nov 22 2026 13:45:50"}  (54 chars)
  char mqttBuffer[STATUS_BUFFER_SIZE];
  snprintf(mqttBuffer, sizeof mqttBuffer, "{\"message\":\"online\",\"fw\":\"%s\"}", FW_VERSION);

  Serial.println(mqttBuffer);

  drainInbound();

  bool published = mqtt.publish(lwtTOPIC, mqttBuffer, MQTT_LWT_RETAIN);
  mqtt.loop();
  delay(10U);
  if (!published) {
    Serial.print("Failed to publish status on topic: ");
    Serial.println(lwtTOPIC);
    recoverConnectionOrRestart("status publish failed", RecoveryImageMode::CONNECT_FAIL);
    return false;
  }

  Serial.print("Published status on topic: ");
  Serial.println(lwtTOPIC);
  return true;
}


#define WIFI_MQTT_TIMEOUT_MS 15000UL
#define SSL_FORCE_REBUILD_AFTER_FAILS 3

bool connectMqtt() {
  const unsigned long timeoutMs = WIFI_MQTT_TIMEOUT_MS;

  if (!SSL_CLIENT_TO_USE) {
    // First time: create the SSL client
    initializeSSL();
  } else {
    // Soft reset: stop any stale SSL session first
    SSL_CLIENT_TO_USE->stop();
    delay(500U);

    // Hard reset only after repeated failures to avoid heap corruption
    if (globalData.mqttReconnectAttempts >= SSL_FORCE_REBUILD_AFTER_FAILS) {
      Serial.println("Too many SSL failures, rebuilding SSL client ...");
      SSL_CLIENT_TO_USE.reset();
      initializeSSL();
    }
  }

  mqtt.setClient(*SSL_CLIENT_TO_USE);
  applyVerificationTime(*SSL_CLIENT_TO_USE);

  Serial.println("MQTT Connecting ....");
  unsigned long start = millis();
  bool deadlineExtended = false;
  while (!mqtt.connected()) {
    if (millis() - start > timeoutMs) {  // timeout
      Serial.printf("MQTT connect timeout (%lu ms)\n", timeoutMs);
      return false;
    }

    static const char* lwtPayload = "{\"message\":\"offline\"}";
    // cleanSession=false -> AWS IoT broker keeps the session for up to 1 hour
    // across disconnects and redelivers queued QoS 1 msgs on reconnect. Without
    // this, any dispense msg the server publishes while we are reconnecting is
    // silently dropped at the broker - which is exactly the "bKash payment went
    // through but nothing dispensed" pattern we were seeing.
    if (mqtt.connect(MACHINE_ID, nullptr, nullptr, lwtTOPIC, MQTT_LWT_QOS, MQTT_LWT_RETAIN, lwtPayload, false)) {
      Serial.println("CONNECTION ESTABLISHED WITH LWT");
      sendCurrentStatus();
      break;
    } else {
      // A certificate rejection ("Certificate is expired or not yet valid.")
      // closes the TLS engine with getWriteError() still SSL_OK; a TCP failure
      // reports SSL_CLIENT_CONNECT_FAIL. If TCP got through and we are not on
      // NTP time, fetch the time again instead of rebooting into the same
      // rejection.
      if (SSL_CLIENT_TO_USE->getWriteError() != SSLClient::SSL_CLIENT_CONNECT_FAIL && clockResyncDue()) {
        SSL_CLIENT_TO_USE->stop();
        if (clockResync()) {
          applyVerificationTime(*SSL_CLIENT_TO_USE);
          // Give the new time a full timeout, but only once per call: a
          // source that keeps "succeeding" (e.g. the modem clock while NTP is
          // down) must not stop this function from timing out, or the retry
          // counter, SSL rebuild and reboot recovery would never run.
          if (!deadlineExtended) {
            start = millis();
            deadlineExtended = true;
          }
        }
      }
      delay(500U);
      yield();
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

bool establishMqttConnection(RecoveryImageMode imageModeOnFailure) {
  if (!isTransportConnected()) {
    Serial.println("Underlying network is down. Reconnecting transport...");

#ifdef USE_WIFI
    if (!tryWifiConnect()) {
      Serial.println("WiFi reconnect failed.");
      if (imageModeOnFailure != RecoveryImageMode::NONE) showConnectingRetrySequence();
      return false;
    }
#else
    if (!tryGsmConnect()) {
      Serial.println("GSM reconnect failed.");
      if (imageModeOnFailure != RecoveryImageMode::NONE) showConnectingRetrySequence();
      return false;
    }
#endif
  }

  if (mqtt.connected()) return true;

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
  if (imageModeOnFailure != RecoveryImageMode::NONE) showConnectingRetrySequence();
  return false;
}

bool publishBuffer(const char* buffer, RecoveryImageMode imageMode = RecoveryImageMode::CONNECT_FAIL) {
  if (!establishMqttConnection(RecoveryImageMode::NONE)) {
    return false;
  }

  drainInbound();

  bool published = mqtt.publish(pubTOPIC, buffer);
  mqtt.loop();

  if (!published) {
    Serial.print("Failed to publish payload on topic: ");
    Serial.println(pubTOPIC);
    recoverConnectionOrRestart("request publish failed", imageMode);
    return false;
  }

  return true;
}

unsigned long lastStatusSentAt = 0UL;
void sendCurrentStatusInLoop() {
  if (millis() - lastStatusSentAt > 30000U) {   // every half minute
    DispenseRequest currentDispReq;
    getDispenseRequest(currentDispReq);
    if (currentDispReq.isExpectingResponse) return;

    GLOBAL_STATE currentState;
    getGlobalState(currentState);
    if (currentState == GLOBAL_STATE::DISPENSE || currentState == GLOBAL_STATE::SYNC || currentState == GLOBAL_STATE::CONNECT) {
      return;
    }

    if (sendCurrentStatus()) {
      lastStatusSentAt = millis();
    }
  }
}
