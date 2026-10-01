#include<EEPROM.h>
#include <PinButton.h>
#include <esp_pm.h>

#include "display.h"

/////////////////
// TOP 4 motor -> 2 dispenser
// BOTTOM 4 motor -> 4 dispenser
/////////////////

// both product buttons require cash acceptance
// cash -> 2 dispense

bool SHOULD_USE_NOTE_ACCEPTOR = true;

enum ProductType { NORMAL = 0, LARGE = 1 };

constexpr int normalFirstCol = 3;
constexpr int normalLastCol = 6;
constexpr int largeFirstCol = 1;
constexpr int largeLastCol = 2;

constexpr int normalProductPerColumn = 20; // maximum NORMAL product in a column
constexpr int largeProductPerColumn = 15;  // maximum LARGE product in a column

int productPerColumn(ProductType pt) {
  return pt == ProductType::NORMAL ? normalProductPerColumn : largeProductPerColumn;
}

/// Each int = 4 bytes; keep gaps for future
#define eeprNormalCol      200
#define eeprNormalTotal    204
#define eeprNormalColCnt   208

#define eeprLargeCol       220
#define eeprLargeTotal     224
#define eeprLargeColCnt    228

constexpr int eeprSize = 256;

/// State for each product type
int normalCol = normalFirstCol;     // current column pointer for NORMAL
int normalTotal = 0;                // total dispensed NORMAL across all time
int normalColDispenseCount = 0;     // dispensed count within current normal column

int largeCol = largeFirstCol;       // current column pointer for LARGE
int largeTotal = 0;                 // total dispensed LARGE
int largeColDispenseCount = 0;      // dispensed count within current large column

int price = 30; // 3 pulse per tk

// Defining Limit Switch input pin for motor rotation
constexpr int limit1 = 21;
constexpr int limit2 = 19;
constexpr int limit3 = 18;
constexpr int limit4 = 13;
constexpr int limit5 = 16; // CAUTION
constexpr int limit6 = 17; // CAUTION

// Defining Motor output pin
constexpr int motor1 = 23;
constexpr int motor2 = 22;
constexpr int motor3 = 4;
constexpr int motor4 = 2;
constexpr int motor5 = 32;
constexpr int motor6 = 33;
constexpr int motor7 = 26;
constexpr int motor8 = 27;

PinButton pushBtn1(GPIO_NUM_34);
PinButton pushBtn2(GPIO_NUM_39);

const MultiButtonConfig resetButtonConfig = {20, 250, 1750};

PinButton rstBtn1(GPIO_NUM_36, &resetButtonConfig); // normal
PinButton rstBtn2(GPIO_NUM_5, &resetButtonConfig); // large

constexpr int noteAcceptorTX = GPIO_NUM_14;   // Cash Acceptor Rx & Tx
constexpr int noteAcceptorRX = GPIO_NUM_35;

unsigned long maxPressTime = 60000;  // Maximum Time for cash acceptor remain ON after push button pressed

void showPushButtonImage() {
  showImage(pushButtonForDispense_t);
}

void setup() {
  Serial.begin(115200);

  composite.init();
  graphics.init();
  showImage(welcome_t);
  delay(2000);

  readStockFromEEPROM();

  pinMode(noteAcceptorRX, INPUT_PULLUP);
  pinMode(noteAcceptorTX, OUTPUT);

  pinMode(limit1, INPUT_PULLUP);
  pinMode(limit2, INPUT_PULLUP);
  pinMode(limit3, INPUT_PULLUP);
  pinMode(limit4, INPUT_PULLUP);
  pinMode(limit5, INPUT_PULLUP);
  pinMode(limit6, INPUT_PULLUP);

  pinMode(motor1, OUTPUT);
  pinMode(motor2, OUTPUT);
  pinMode(motor3, OUTPUT);
  pinMode(motor4, OUTPUT);
  pinMode(motor5, OUTPUT);
  pinMode(motor6, OUTPUT);
  pinMode(motor7, OUTPUT);
  pinMode(motor8, OUTPUT);

  digitalWrite(motor1, LOW);
  digitalWrite(motor2, LOW);
  digitalWrite(motor3, LOW);
  digitalWrite(motor4, LOW);
  digitalWrite(motor5, LOW);
  digitalWrite(motor6, LOW);
  digitalWrite(motor7, LOW);
  digitalWrite(motor8, LOW);

  digitalWrite(noteAcceptorTX, LOW);

  delay(3000);
  showPushButtonImage();
}


void readStockFromEEPROM() {
  EEPROM.begin(eeprSize);

  normalCol = EEPROM.readInt(eeprNormalCol);
  normalTotal = EEPROM.readInt(eeprNormalTotal);
  normalColDispenseCount = EEPROM.readInt(eeprNormalColCnt);

  largeCol = EEPROM.readInt(eeprLargeCol);
  largeTotal = EEPROM.readInt(eeprLargeTotal);
  largeColDispenseCount = EEPROM.readInt(eeprLargeColCnt);

  SHOULD_USE_NOTE_ACCEPTOR = true;

  EEPROM.end();
  delay(250);

  // Clamp invalid/empty values
  if (normalCol < normalFirstCol || normalCol > normalLastCol) normalCol = normalFirstCol;
  if (normalTotal < 0) normalTotal = 0;
  if (normalColDispenseCount < 0 || normalColDispenseCount > normalProductPerColumn) normalColDispenseCount = 0;

  if (largeCol < largeFirstCol || largeCol > largeLastCol) largeCol = largeFirstCol;
  if (largeTotal < 0) largeTotal = 0;
  if (largeColDispenseCount < 0 || largeColDispenseCount > largeProductPerColumn) largeColDispenseCount = 0;

  Serial.printf("EEPROM NORMAL: col=%d total=%d colCount=%d\n",
    normalCol, normalTotal, normalColDispenseCount);
  Serial.printf("EEPROM LARGE:  col=%d total=%d colCount=%d\n",
    largeCol, largeTotal, largeColDispenseCount);
}

void writeStockToEEPROM() {
  EEPROM.begin(eeprSize);

  EEPROM.writeInt(eeprNormalCol, normalCol);
  EEPROM.writeInt(eeprNormalTotal, normalTotal);
  EEPROM.writeInt(eeprNormalColCnt, normalColDispenseCount);
  delay(50);

  EEPROM.writeInt(eeprLargeCol, largeCol);
  EEPROM.writeInt(eeprLargeTotal, largeTotal);
  EEPROM.writeInt(eeprLargeColCnt, largeColDispenseCount);
  delay(50);

  EEPROM.commit();
  EEPROM.end();

  Serial.printf("WROTE NORMAL: col=%d total=%d colCount=%d | LARGE: col=%d total=%d colCount=%d\n",
    normalCol, normalTotal, normalColDispenseCount,
    largeCol, largeTotal, largeColDispenseCount);
}

void reset(ProductType pt) {
  String ptName = pt == ProductType::NORMAL ? "Normal" : "Large";
  Serial.print("Resetting ");
  Serial.println(ptName);
  if (pt == ProductType::NORMAL) {
    normalCol = normalFirstCol;
    normalTotal = 0;
    normalColDispenseCount = 0;
  } else if (pt == ProductType::LARGE) {
    largeCol = largeFirstCol;
    largeTotal = 0;
    largeColDispenseCount = 0;
  }
  writeStockToEEPROM();
  showImage(syncComplete_t);
  Serial.print(ptName);
  Serial.println(" stock reset done.\n");
  delay(2000);
  showPushButtonImage();
}

bool isStockEmpty(ProductType pt) {
  int firstCol = pt == ProductType::NORMAL ? normalFirstCol : largeFirstCol;
  int lastCol = pt == ProductType::NORMAL ? normalLastCol : largeLastCol;
  int totalDispense = pt == ProductType::NORMAL ? normalTotal : largeTotal;
  int columnCount = lastCol - firstCol + 1;

  int required = 2;

  int capacity = columnCount * productPerColumn(pt);
  return (totalDispense + required) > capacity;
}

void checkResetButtons() {
  if (rstBtn1.isLongClick()) reset(ProductType::NORMAL);
  if (rstBtn2.isLongClick()) reset(ProductType::LARGE);
}

bool shownImgForAllEmpty = false;

void loop() {
  bool isNormalEmpty = isStockEmpty(ProductType::NORMAL);
  bool isLargeEmpty = isStockEmpty(ProductType::LARGE);

  rstBtn1.update();
  rstBtn2.update();

  if (isNormalEmpty && isLargeEmpty) {
    if (!shownImgForAllEmpty) {
      showImage(pleaseRefill_t);
      shownImgForAllEmpty = true;
    }

    checkResetButtons();
    return;
  }
  shownImgForAllEmpty = false;

  checkResetButtons();

  pushBtn1.update();
  pushBtn2.update();

  // NORMAL button
  if (pushBtn1.isSingleClick() || pushBtn1.isLongClick()) {
    if (isNormalEmpty) {
      showImage(pleaseRefill_t);
      Serial.println("NORMAL empty");
      delay(3000);
      showPushButtonImage();
      return;
    }

    Serial.println("Button NORMAL Pressed");
    showImage(insertTenTaka_t);
    noteAcceptAndVend(ProductType::NORMAL);
    return;
  }

  // LARGE button
  if (pushBtn2.isSingleClick() || pushBtn2.isLongClick()) {
    if (isLargeEmpty) {
      showImage(pleaseRefill_t);
      Serial.println("LARGE empty");
      delay(3000);
      showPushButtonImage();
      return;
    }

    Serial.println("Button LARGE Pressed");
    showImage(insertTenTaka_t);
    noteAcceptAndVend(ProductType::LARGE);
    return;
  }
}

void noteAcceptAndVend(ProductType pt) {
  Serial.println("Waiting for cash acceptor pulses...");

  int credit = 0;
  bool hasStartedProcessing = false;
  digitalWrite(noteAcceptorTX, HIGH);
  unsigned long startTime = millis();

  while (true) {
    int pwm_value = pulseIn(noteAcceptorRX, LOW);
    if (pwm_value != 0) {
      startTime = millis();
      if (!hasStartedProcessing) {
        showImage(noteProcessing_t);
        hasStartedProcessing = true;
      }
      credit++;
      Serial.print(credit);
      Serial.print(",");
    }

    if (credit >= (price - 5)) {
      digitalWrite(noteAcceptorTX, LOW);
      Serial.println("");
      Serial.println("Credited! Acceptor Disabled");
      showImage(noteAccepted_t);

      delay(600);
      showImage(dispensing_t);

      bool ok1 = vendOne(pt);
      delay(1000);
      bool ok2 = vendOne(pt);
      delay(250);

      showImage(ok1 && ok2 ? collectProduct_t : pleaseRefill_t);
      delay(1000);
      showPushButtonImage();
      break;
    }

    if (millis() - startTime >= maxPressTime) {
      Serial.println();
      showImage(timeOut_t);
      delay(1200);
      showPushButtonImage();
      Serial.println("Timeout! Acceptor Disabled");
      digitalWrite(noteAcceptorTX, LOW);
      break;
    }
  }
}

void advance(ProductType pt) {
  if (pt == ProductType::NORMAL) {
    normalColDispenseCount = 0;
    normalCol++;
    if (normalCol > normalLastCol) normalCol = normalFirstCol;
  } else {
    largeColDispenseCount = 0;
    largeCol++;
    if (largeCol > largeLastCol) largeCol = largeFirstCol;
  }
}

// normal (1 motor to dispense 1 product)
void dispenseNormal(int m, int l) {
  digitalWrite(m, HIGH);
  delay(1000); // wait for motor to et out of rotation sensor first

  Serial.print("Motor Pin: ");
  Serial.println(m);
  Serial.print("Rotation Pin: ");
  Serial.println(l);

  int h = digitalRead(l);

  unsigned long dispenseStartMillis = millis();

  while (h == LOW && millis() - dispenseStartMillis < 10000) {
    delay(25);
    h = digitalRead(l);

    if (h != LOW) {
      digitalWrite(m, LOW);
    }

    Serial.print(h);
  }

  // timeout
  digitalWrite(m, LOW);

  Serial.println("");
}

// 2 motor to dispense 1 product
void dispenseLarge(int m1, int m2, int l1, int l2) {
  digitalWrite(m1, HIGH);
  digitalWrite(m2, HIGH);
  delay(1000); // wait for motor to et out of rotation sensor first

  Serial.print("Motors pins: ");
  Serial.print(m1);
  Serial.print(", ");
  Serial.println(m2);
  Serial.print("Rotation pins: ");
  Serial.println(l1);
  Serial.print(", ");
  Serial.println(l2);

  int h0 = digitalRead(l1);
  int h1 = digitalRead(l2);

  unsigned long dispenseStartMillis = millis();

  while ((h0 == LOW || h1 == LOW) && (millis() - dispenseStartMillis < 10000)) {
    delay(25);
    h0 = digitalRead(l1);
    h1 = digitalRead(l2);

    if (h0 != LOW) {
      digitalWrite(m1, LOW);
    }
    if (h1 != LOW) {
      digitalWrite(m2, LOW);
    }

    Serial.print(h0);
    Serial.print(h1);
  }

  // timeout
  digitalWrite(m1, LOW);
  digitalWrite(m2, LOW);

  Serial.println("");
}

bool vendOne(ProductType pt) {
  int col = pt == ProductType::NORMAL ? normalCol : largeCol;
  Serial.printf("Vending %s from column %d\n", (pt == ProductType::NORMAL ? "NORMAL" : "LARGE"), col);

  // Dispense by column mapping
  switch (col) {
  case 1: dispenseLarge(motor1, motor2, limit1, limit2); break;
  case 2: dispenseLarge(motor3, motor4, limit3, limit4); break;
  case 3: dispenseNormal(motor5, limit5); break;
  case 4: dispenseNormal(motor6, limit5); break;
  case 5: dispenseNormal(motor7, limit6); break;
  case 6: dispenseNormal(motor8, limit6); break;
  default: return false;
  }

  // Update per-type counters
  if (pt == ProductType::NORMAL) {
    normalColDispenseCount++;
    normalTotal++;
    if (normalColDispenseCount >= normalProductPerColumn) advance(pt);
  } else {
    largeColDispenseCount++;
    largeTotal++;
    if (largeColDispenseCount >= largeProductPerColumn) advance(pt);
  }

  writeStockToEEPROM();
  Serial.println("Dispense Done!");
  return true;
}
