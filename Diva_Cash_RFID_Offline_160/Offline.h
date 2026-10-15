#pragma once

#include <EEPROM.h>

// Local replacement for the backend's stock count and column selection.
// Use the existing offline EEPROM layout: column, total vends, column vends.
constexpr int PRODUCTS_PER_COLUMN = 20;
constexpr int OFFLINE_EEPROM_SIZE = 128;
constexpr int OFFLINE_COLUMN_ADDR = 100;
constexpr int OFFLINE_COUNT_ADDR = 110;
constexpr int OFFLINE_COLUMN_COUNT_ADDR = 120;

int offlineColumn = 1;
int offlineDispenseCount = 0;
int offlineColumnDispenseCount = 0;

void discardRfidReads() {
#if defined(HAS_RFID_MODULE)
  for (uint8_t i = 0; i < 4; i++) {
    uint32_t tag = rdm6300.get_new_tag_id();
    if (tag != 0) {
      lastRfidTime = millis();
    }
  }
#endif
}

void waitOffline(unsigned long duration) {
  unsigned long start = millis();
  while (millis() - start < duration) {
    discardRfidReads();
    delay(5U);
  }
}

void flushRfidReader() {
  for (uint8_t i = 0; i < 4; i++) {
    discardRfidReads();
    delay(5U);
  }
  lastRfidTime = millis();
}

void loadOfflineStock() {
  EEPROM.begin(OFFLINE_EEPROM_SIZE);
  offlineColumn = EEPROM.readInt(OFFLINE_COLUMN_ADDR);
  offlineDispenseCount = EEPROM.readInt(OFFLINE_COUNT_ADDR);
  offlineColumnDispenseCount = EEPROM.readInt(OFFLINE_COLUMN_COUNT_ADDR);
  EEPROM.end();

  const int columnCount = (MACHINE_SIZE + PRODUCTS_PER_COLUMN - 1) / PRODUCTS_PER_COLUMN;
  globalData.hasStock = offlineColumn >= 1 && offlineColumn <= columnCount &&
                       offlineColumn <= 8 && offlineDispenseCount >= 0 &&
                       offlineDispenseCount < MACHINE_SIZE &&
                       offlineColumnDispenseCount >= 0 &&
                       offlineColumnDispenseCount < PRODUCTS_PER_COLUMN;
  Serial.printf("Local stock: column %d, dispensed %d/%d\n",
                offlineColumn, offlineDispenseCount, MACHINE_SIZE);
}

void saveOfflineStock() {
  EEPROM.begin(OFFLINE_EEPROM_SIZE);
  EEPROM.writeInt(OFFLINE_COLUMN_ADDR, offlineColumn);
  EEPROM.writeInt(OFFLINE_COUNT_ADDR, offlineDispenseCount);
  EEPROM.writeInt(OFFLINE_COLUMN_COUNT_ADDR, offlineColumnDispenseCount);
  EEPROM.commit();
  EEPROM.end();
}

void refillOfflineStock() {
  offlineColumn = 1;
  offlineDispenseCount = 0;
  offlineColumnDispenseCount = 0;
  saveOfflineStock();
  globalData.hasStock = true;
}

void showOfflineStatus() {
  if (globalData.hasStock) {
    setGlobalState(GLOBAL_STATE::IDLE);
    showModuleImage();
  } else {
    setGlobalState(GLOBAL_STATE::EMPTY_STOCK);
    showImage(refill_t);
  }
}

#if defined(IS_DEVELOPMENT)
void printOfflineStockDebug() {
  int remainingStock = MACHINE_SIZE - offlineDispenseCount;

  Serial.println("\n========== STOCK DEBUG ==========");
  Serial.printf("Machine ID          : %s\n", MACHINE_ID);
  Serial.printf("Total Capacity      : %d\n", MACHINE_SIZE);
  Serial.printf("Total Dispensed     : %d\n", offlineDispenseCount);
  Serial.printf("Remaining Stock     : %d\n", remainingStock);
  Serial.printf("Current Column      : %d / %d\n",
                offlineColumn, 3);
  Serial.printf("Dispensed in Column : %d / %d\n",
                offlineColumnDispenseCount, PRODUCTS_PER_COLUMN);
  Serial.printf("Has Stock           : %s\n",
                globalData.hasStock ? "YES" : "NO");
  Serial.println("=================================\n");
}
#endif
void dispenseOffline() {
  if (!globalData.hasStock) return;
  setGlobalState(GLOBAL_STATE::DISPENSE);
  showImage(dispensing_t);
  dispenseProcess(offlineColumn);

  offlineDispenseCount++;
  offlineColumnDispenseCount++;
  if (offlineColumnDispenseCount >= PRODUCTS_PER_COLUMN) {
    offlineColumn++;
    offlineColumnDispenseCount = 0;
  }
  if (offlineColumn > (MACHINE_SIZE + PRODUCTS_PER_COLUMN - 1) / PRODUCTS_PER_COLUMN) {
    offlineColumn = 1;
  }
  saveOfflineStock();
  globalData.hasStock = offlineDispenseCount < MACHINE_SIZE;
#if defined(IS_DEVELOPMENT)
  printOfflineStockDebug();
#endif
  showImage(collectProduct_t);
  flushRfidReader();
  showOfflineStatus();
}
