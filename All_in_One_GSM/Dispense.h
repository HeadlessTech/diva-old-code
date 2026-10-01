// Dispense.h
#pragma once

static void (*_mqttLoopCallback)() = nullptr;
inline void setMqttLoopCallback(void (*fn)()) { _mqttLoopCallback = fn; }

constexpr int motorPins[8] = {
  GPIO_NUM_23, GPIO_NUM_22,
  GPIO_NUM_4, GPIO_NUM_2,
  GPIO_NUM_32, GPIO_NUM_33,
  GPIO_NUM_26, GPIO_NUM_27,
};
constexpr int motorLimitSwitches[4] = {
  GPIO_NUM_21, GPIO_NUM_19,
  GPIO_NUM_18, GPIO_NUM_13
};

void runMotorSpring(int motorPin, int limitSwitch) {  //for spring type dispense chember
  delay(20U);
  digitalWrite(motorPin, HIGH);
  for (int i = 0; i < 10; i++) { if (_mqttLoopCallback) _mqttLoopCallback(); delay(100U); }

  Serial.print("MotorPin: ");
  Serial.println(motorPin);
  Serial.print("Limit switch: ");
  Serial.println(limitSwitch);

  int h = digitalRead(limitSwitch);
  Serial.println(h);
  while (h == LOW) {
    delay(50U);
    if (_mqttLoopCallback) _mqttLoopCallback(); // keep MQTT/SSL alive during spin
    h = digitalRead(limitSwitch);
    Serial.print(h);
  }
  if (h != LOW) {
    digitalWrite(motorPin, LOW);
    Serial.print("\nmotor ");
    Serial.print(motorPin);
    Serial.println(" stop");
  }
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
