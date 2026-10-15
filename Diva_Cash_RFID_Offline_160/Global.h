// Global.h
#pragma once

#include <freertos/portmacro.h>
#include <freertos/semphr.h>

struct GlobalData {
  bool hasStock = false;
};
GlobalData globalData = {};

enum GLOBAL_STATE : uint8_t {
  EMPTY_STOCK,
  IDLE,
  DISPENSE,
  CASH,
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
