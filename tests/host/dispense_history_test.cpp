// Host test for the dispense-number history in All_in_One/Global.h: the
// guard that stops a redelivered dispense command (broker redelivery, server
// resends, a reboot mid-dispense) from dispensing twice.
//
// Run with tools/test-host.sh.
#include <cstdio>
#include <cstring>

// The firmware's Serial, reduced to what Global.h uses (output discarded).
struct FakeSerial {
  template <typename... Args>
  void printf(const char*, Args...) {}
  void println(const char*) {}
} Serial;

#define IS_TWO_GROUP 1  // also build the two-group helpers
#include "../../All_in_One/Global.h"

static int passed = 0, failed = 0;
#define CHECK(cond, what)                                   \
  do {                                                      \
    if (cond) {                                             \
      passed++;                                             \
      std::printf("  PASS  %s\n", what);                    \
    } else {                                                \
      failed++;                                             \
      std::printf("  FAIL  %s  (%s:%d)\n", what, __FILE__, __LINE__); \
    }                                                       \
  } while (0)

static int last() {
  int n = -1;
  getLastDispenseNo(n);
  return n;
}

// Power loss: RAM is gone, NVS (Preferences::storage) stays.
static void reboot() {
  LAST_DISPENSE_COUNT = 0;
  LAST_DISPENSE_POS = 0;
  memset(LAST_DISPENSES, 0, sizeof LAST_DISPENSES);
  dispenseNvs = Preferences();
  loadDispenseHistory();
}

static bool mutexesFree() {
  return dispenseNoMutex->held == 0 && gStateMutex->held == 0 && dispenseReqMutex->held == 0;
}

int main() {
  std::printf("== dispense-number history (Global.h)\n");
  loadDispenseHistory();
  CHECK(last() == 0, "starts empty: last dispense number is 0");
  CHECK(!isDispenseInHistory(5), "an unknown number is not in the history");

  for (int n = 1; n <= 5; n++) setLastDispenseNo(n);
  CHECK(last() == 5, "last number is the most recent one");
  CHECK(isDispenseInHistory(3) && isDispenseInHistory(5), "recent numbers are found");
  CHECK(!isDispenseInHistory(6), "a new number is not found");

  setLastDispenseNo(5);
  CHECK(LAST_DISPENSE_COUNT == 5, "repeating the last number adds nothing");
  CHECK(mutexesFree(), "the repeat path releases the mutex");

  for (int n = 6; n <= 150; n++) setLastDispenseNo(n);
  CHECK(LAST_DISPENSE_COUNT == 100, "keeps the last 100 numbers");
  CHECK(isDispenseInHistory(51) && isDispenseInHistory(150), "the newest 100 are found after wrap-around");
  CHECK(!isDispenseInHistory(50), "older numbers are forgotten");
  CHECK(last() == 150, "last number is right after wrap-around");

  reboot();
  CHECK(last() == 150, "after a reboot the last number is restored from NVS");
  CHECK(isDispenseInHistory(51) && !isDispenseInHistory(50), "after a reboot the same 100 numbers are known");
  setLastDispenseNo(151);
  CHECK(isDispenseInHistory(151) && !isDispenseInHistory(51), "after a reboot, new numbers continue the ring");

  // A corrupted record (impossible count) must give an empty history, never
  // an out-of-range index.
  {
    int bad[LAST_DISPENSE_HISTORY_SIZE + 2] = {500, 7};
    Preferences p;
    p.begin("dispHist");
    p.putBytes("hist", bad, sizeof bad);
  }
  reboot();
  CHECK(LAST_DISPENSE_COUNT == 0 && last() == 0, "a corrupted NVS record starts empty");

  // A record of the wrong size (e.g. written by a firmware with another
  // history size) is ignored.
  {
    int small[3] = {1, 1, 42};
    Preferences p;
    p.begin("dispHist");
    p.putBytes("hist", small, sizeof small);
  }
  reboot();
  CHECK(LAST_DISPENSE_COUNT == 0, "a record of the wrong size is ignored");
  CHECK(mutexesFree(), "no mutex is left taken");

  std::printf("== two-group stock (Global.h)\n");
  sizeStock.belt = 3;
  sizeStock.panty = 0;
  CHECK(getStockForSize(SIZE_BELT) == 3 && getStockForSize(SIZE_PANTY) == 0, "stock per group");
  CHECK(getStockForSize("OTHER") == 0 && getStockForSize(nullptr) == 0, "unknown or no group has no stock");

  std::printf("== machine state (Global.h)\n");
  setGlobalState(GLOBAL_STATE::DISPENSE);
  GLOBAL_STATE s;
  getGlobalState(s);
  CHECK(s == GLOBAL_STATE::DISPENSE, "state is stored");
  DispenseRequest r;
  r.isExpectingResponse = true;
  r.requestedAt = 1234;
  setDispenseRequest(r);
  DispenseRequest r2;
  getDispenseRequest(r2);
  CHECK(r2.isExpectingResponse && r2.requestedAt == 1234, "pending request is stored");
  CHECK(mutexesFree(), "no mutex is left taken");

  std::printf("\npassed: %d  failed: %d\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
