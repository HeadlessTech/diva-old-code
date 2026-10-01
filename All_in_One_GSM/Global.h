// Global.h
#pragma once

#include <freertos/portmacro.h>
#include <freertos/semphr.h>

struct GlobalData {
  bool hasStock = false;
  bool isServerCommunication = false;
  bool useWifi = false; // false = GSM
  int strength = -120;
  int mqttReconnectAttempts = 0;   // counts reconnect failures
  unsigned long latestConnectionTrial = 0UL;
};

GlobalData globalData = {};

// ------------------------------------------------------------------
// Dispense-number history (guards against duplicate MQTT messages)
// ------------------------------------------------------------------

// Ring-buffer of the last N dispense numbers we have processed
static constexpr uint8_t LAST_DISPENSE_HISTORY_SIZE = 20;
static int LAST_DISPENSES[LAST_DISPENSE_HISTORY_SIZE] = { 0 };
static int LAST_DISPENSE_COUNT = 0; // how many valid entries (<= SIZE)
static int LAST_DISPENSE_POS = 0; // next insert index
SemaphoreHandle_t dispenseNoMutex = xSemaphoreCreateMutex();

// Returns the most-recent dispenseNo via outDispenseNo, or 0 if history is empty
bool getLastDispenseNo(int& outDispenseNo) {
  if (xSemaphoreTake(dispenseNoMutex, portMAX_DELAY) != pdTRUE) return false;
  if (LAST_DISPENSE_COUNT == 0) {
    outDispenseNo = 0;
  } else {
    int idx = (LAST_DISPENSE_POS + LAST_DISPENSE_HISTORY_SIZE - 1) % LAST_DISPENSE_HISTORY_SIZE;
    outDispenseNo = LAST_DISPENSES[idx];
  }
  xSemaphoreGive(dispenseNoMutex);
  return true;
}

// Pushes new dispense number into history (no-op if identical to most recent)
bool setLastDispenseNo(int newDispenseNo) {
  if (xSemaphoreTake(dispenseNoMutex, portMAX_DELAY) != pdTRUE) return false;
  // avoid duplicate consecutive entries
  if (LAST_DISPENSE_COUNT > 0) {
    int lastIdx = (LAST_DISPENSE_POS + LAST_DISPENSE_HISTORY_SIZE - 1) % LAST_DISPENSE_HISTORY_SIZE;
    if (LAST_DISPENSES[lastIdx] == newDispenseNo) {
      xSemaphoreGive(dispenseNoMutex);
      return true;
    }
  }

  LAST_DISPENSES[LAST_DISPENSE_POS] = newDispenseNo;
  LAST_DISPENSE_POS = (LAST_DISPENSE_POS + 1) % LAST_DISPENSE_HISTORY_SIZE;
  if (LAST_DISPENSE_COUNT < LAST_DISPENSE_HISTORY_SIZE) LAST_DISPENSE_COUNT++;

  xSemaphoreGive(dispenseNoMutex);
  return true;
}

// Returns true if the given dispenseNo is present in the recent history
bool isDispenseInHistory(int dispenseNo) {
  if (xSemaphoreTake(dispenseNoMutex, portMAX_DELAY) != pdTRUE) return false;
  bool found = false;
  for (int i = 0; i < LAST_DISPENSE_COUNT; i++) {
    int idx = (LAST_DISPENSE_POS + LAST_DISPENSE_HISTORY_SIZE - LAST_DISPENSE_COUNT + i) % LAST_DISPENSE_HISTORY_SIZE;
    if (LAST_DISPENSES[idx] == dispenseNo) {
      found = true;
      break;
    }
  }
  xSemaphoreGive(dispenseNoMutex);
  return found;
}

// ---------------------------------------------------------------------------
// Machine state
// ---------------------------------------------------------------------------

enum GLOBAL_STATE : uint8_t {
  EMPTY_STOCK,
  IDLE,
  DISPENSE,
  CONNECT,
  SYNC
};

static GLOBAL_STATE globalState = GLOBAL_STATE::EMPTY_STOCK;
SemaphoreHandle_t gStateMutex = xSemaphoreCreateMutex();
bool getGlobalState(GLOBAL_STATE& outState) {
  if (xSemaphoreTake(gStateMutex, portMAX_DELAY) != pdTRUE) return false;
  outState = globalState;
  xSemaphoreGive(gStateMutex);
  return true;
}
bool setGlobalState(GLOBAL_STATE newState) {
  if (xSemaphoreTake(gStateMutex, portMAX_DELAY) != pdTRUE) return false;
  globalState = newState;
  xSemaphoreGive(gStateMutex);
  return true;
}

// ---------------------------------------------------------------------------
// Pending dispense request
// ---------------------------------------------------------------------------

struct DispenseRequest {
  bool isExpectingResponse = false;
  unsigned long requestedAt = 0;
};

static DispenseRequest globalDispenseRequest = {};
SemaphoreHandle_t dispenseReqMutex = xSemaphoreCreateMutex();
bool getDispenseRequest(DispenseRequest& outState) {
  if (dispenseReqMutex == nullptr) return false;
  if (xSemaphoreTake(dispenseReqMutex, portMAX_DELAY) != pdTRUE) return false;
  outState = globalDispenseRequest;
  xSemaphoreGive(dispenseReqMutex);
  return true;
}
bool setDispenseRequest(const DispenseRequest& newState) {
  if (dispenseReqMutex == nullptr) return false;
  if (xSemaphoreTake(dispenseReqMutex, portMAX_DELAY) != pdTRUE) return false;
  globalDispenseRequest = newState;
  xSemaphoreGive(dispenseReqMutex);
  return true;
}





