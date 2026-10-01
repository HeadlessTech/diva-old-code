// Global.h
#pragma once

#include <freertos/portmacro.h>
#include <freertos/semphr.h>
#include <Preferences.h>

struct GlobalData {
  bool hasStock = false;
  bool isServerCommunication = false;
  bool useWifi = true;   // false = GSM
  int  strength = -120;   // last measured RSSI
  int  mqttReconnectAttempts = 0;      // consecutive reconnect failures
  unsigned long latestConnectionTrial = 0UL;
};

GlobalData globalData = {};

// ---------------------------------------------------------------------------
// Dispense-number history  (guards against duplicate MQTT messages)
// ---------------------------------------------------------------------------

// Ring-buffer of the last N dispense numbers we have processed.
#define LAST_DISPENSE_HISTORY_SIZE 100
static int LAST_DISPENSES[LAST_DISPENSE_HISTORY_SIZE] = { 0 };
static int LAST_DISPENSE_COUNT = 0; // how many valid entries (<= SIZE)
static int LAST_DISPENSE_POS = 0;   // next insert index
SemaphoreHandle_t dispenseNoMutex = xSemaphoreCreateMutex();

// Persistence: load + save the ring buffer + count/pos across reboots so a
// broker replay after a reboot cannot re-dispense a recently processed item.
Preferences dispenseNvs;

// Serialized the array + count + pos into one 3*SIZE-word blob and writes it
// to NVS. Caller MUST hold dispenseNoMutex
void persistDispenseHistory_nolock() {
  // header layout: [count, pos]
  // payload: LAST_DISPENSE[]
  int header[2] = { LAST_DISPENSE_COUNT, LAST_DISPENSE_POS };
  size_t need = (LAST_DISPENSE_HISTORY_SIZE + 2) * sizeof(int);
  uint8_t blob[(LAST_DISPENSE_HISTORY_SIZE + 2) * sizeof(int)];
  memcpy(blob, header, sizeof(header));
  memcpy(blob + sizeof(header), LAST_DISPENSES, sizeof(LAST_DISPENSES));
  dispenseNvs.putBytes("hist", blob, need);
  dispenseNvs.putInt("lastNo", 
    (LAST_DISPENSE_COUNT == 0) 
      ? 0
      : LAST_DISPENSES[(LAST_DISPENSE_POS + LAST_DISPENSE_HISTORY_SIZE - 1) % LAST_DISPENSE_HISTORY_SIZE]
  );
}

// Call ONCE at boot (before MQTT connects / onMqttMessage can fire).
void loadDispenseHistory() {
  dispenseNvs.begin("dispHist", false);
  size_t need = (LAST_DISPENSE_HISTORY_SIZE + 2) * sizeof(int);
  uint8_t blob[(LAST_DISPENSE_HISTORY_SIZE + 2) * sizeof(int)];
  size_t got = dispenseNvs.getBytes("hist", blob, need);
  if (got == need) {
    int header[2];
    memcpy(header, blob, sizeof(header));
    LAST_DISPENSE_COUNT = header[0];
    LAST_DISPENSE_POS   = header[1];
    memcpy(LAST_DISPENSES, blob + sizeof(header), sizeof(LAST_DISPENSES));
    // Sanity: bad values -> reset to empty so we never index out of bounds
    if (
      LAST_DISPENSE_COUNT < 0 || LAST_DISPENSE_COUNT > LAST_DISPENSE_HISTORY_SIZE ||
      LAST_DISPENSE_POS < 0 || LAST_DISPENSE_POS >= LAST_DISPENSE_HISTORY_SIZE
    ) {
      LAST_DISPENSE_COUNT = 0;
      LAST_DISPENSE_POS = 0;
      memset(LAST_DISPENSES, 0, sizeof(LAST_DISPENSES));
    }
    if (LAST_DISPENSE_COUNT > 0) {
      int idx = (LAST_DISPENSE_POS + LAST_DISPENSE_HISTORY_SIZE - 1) % LAST_DISPENSE_HISTORY_SIZE;
      Serial.printf("[dispHist] loaded %d entries; lastDispenseNo=%d\n", LAST_DISPENSE_COUNT, LAST_DISPENSES[idx]);
    } else {
      Serial.println("[dispHist] loaded empty");
    }
  } else {
    Serial.printf("[dispHist] no prior record (got=%u), starting clean\n", (unsigned)got);
    LAST_DISPENSE_COUNT = 0;
    LAST_DISPENSE_POS = 0;
  }
}

// Returns the most-recent dispenseNo via outDispenseNo, or 0 if history is empty.
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

  // Persist to NVS while still holding the mutex so the saved blob never
  // reflects a half-applied ring-buffer mutation.
  persistDispenseHistory_nolock();

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
  SYNC,
  AWAITING_RFID,  // waiting for RFID tap after group button press
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

// ---------------------------------------------------------------------------
// Dual-group state (160-cap machines with top/bottom dispenser groups)
// ---------------------------------------------------------------------------
#define SIZE_BELT    "BELT"   // sizeId for top group: columns 1-2 (LARGE, dual-motor)
#define SIZE_PANTY   "PANTY"  // sizeId for bottom group: columns 3-6 (NORMAL, single-motor)
#define GROUP_RFID_TIMEOUT_MS 15000UL  // 15s timeout after button press

static const char* selectedGroup = nullptr;     // nullptr = none, "BELT" or "PANTY"
static unsigned long groupSelectedAt = 0;  // millis() when group was selected

// ---------------------------------------------------------------------------
// Per-size stock counters (populated from sync response "stock" object)
// ---------------------------------------------------------------------------
struct SizeStock {
  uint16_t belt  = 0;
  uint16_t panty = 0;
};

static SizeStock sizeStock = {};

// Returns the stock count for the given sizeId, or 0 if unknown
inline uint16_t getStockForSize(const char* sizeId) {
  if (sizeId == nullptr) return 0;
  if (strcmp(sizeId, SIZE_BELT) == 0)  return sizeStock.belt;
  if (strcmp(sizeId, SIZE_PANTY) == 0) return sizeStock.panty;
  return 0;
}

