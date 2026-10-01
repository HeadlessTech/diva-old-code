// Dispense.h
#pragma once

constexpr int motorPins[8] = {
  GPIO_NUM_23, GPIO_NUM_22,
  GPIO_NUM_4, GPIO_NUM_2,
  GPIO_NUM_32, GPIO_NUM_33,
  GPIO_NUM_26, GPIO_NUM_27,
};

constexpr int motorLimitSwitches[4] = {
  GPIO_NUM_21, GPIO_NUM_19,
  GPIO_NUM_18, GPIO_NUM_13,
};

void runMotorSpring(int motorPin, int limitSwitch) {
  static const unsigned long LIMIT_SWITCH_TIMEOUT_MS = 3000UL;

  delay(20U);
  digitalWrite(motorPin, HIGH);
  delay(1000U);

  Serial.print("MotorPin: ");
  Serial.println(motorPin);
  Serial.print("Limit switch: ");
  Serial.println(limitSwitch);

  unsigned long waitStart = millis();
  int h = digitalRead(limitSwitch);
  Serial.println(h);
  while (h == LOW) {
    if (millis() - waitStart >= LIMIT_SWITCH_TIMEOUT_MS) {
      digitalWrite(motorPin, LOW);
      Serial.print("\nmotor ");
      Serial.print(motorPin);
      Serial.println(" TIMEOUT - limit switch never tripped, motor forced off");
      return;
    }
    delay(50U);
    h = digitalRead(limitSwitch);
    Serial.print(h);
  }
  digitalWrite(motorPin, LOW);
  Serial.print("\nmotor ");
  Serial.print(motorPin);
  Serial.println(" stop");
}

void dispenseProcess(int col) {
  Serial.println("Dispensing... ");
  switch (col) {
    case 1:
      runMotorSpring(motorPins[0], motorLimitSwitches[0]);
      break;
    case 2:
      runMotorSpring(motorPins[1], MACHINE_SIZE <= 60 ? motorLimitSwitches[1] : motorLimitSwitches[0]);
      break;
    case 3:
      runMotorSpring(motorPins[2], MACHINE_SIZE <= 60 ? motorLimitSwitches[2] : motorLimitSwitches[1]);
      break;
    case 4:
      runMotorSpring(motorPins[3], motorLimitSwitches[1]);
      break;
    case 5:
      runMotorSpring(motorPins[4], motorLimitSwitches[2]);
      break;
    case 6:
      // Pin 33 doubles as a crystal oscillator input; some timer operations
      // force it back to INPUT mode, so we re-assert OUTPUT before driving it.
      pinMode(motorPins[5], OUTPUT);
      runMotorSpring(motorPins[5], motorLimitSwitches[2]);
      break;
    case 7:
      runMotorSpring(motorPins[6], motorLimitSwitches[3]);
      break;
    case 8:
      runMotorSpring(motorPins[7], motorLimitSwitches[3]);
      break;
    default:
      break;
  }
  Serial.println("Dispense Done!");
}
