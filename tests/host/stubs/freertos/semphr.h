// Host stand-in for FreeRTOS mutexes (tests only). Counts takes and gives
// so a test can check that every code path releases the mutex.
#pragma once
#include <cstdint>

struct FakeMutex {
  int held = 0;      // > 0 while taken
  int takes = 0;
};
typedef FakeMutex* SemaphoreHandle_t;
typedef int BaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xffffffffu

inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new FakeMutex(); }
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t m, uint32_t) {
  if (m->held) return pdFALSE;  // a real mutex would block forever here
  m->held++;
  m->takes++;
  return pdTRUE;
}
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t m) {
  if (m->held) m->held--;
  return pdTRUE;
}
