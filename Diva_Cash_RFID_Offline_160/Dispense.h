// Dispense.h
#pragma once

// Callback consumes RFID reads during motor movement to prevent queued vends.
static void (*_rfidPollCallback)() = nullptr;
inline void setRfidPollCallback(void (*fn)()) { _rfidPollCallback = fn; }

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

void runMotorSpring(int motorPin, int limitSwitch) {  // spring-type dispense chamber
  delay(20U);
  digitalWrite(motorPin, HIGH);
  // one-second startup delay, servicing RFID meanwhile.
  for (uint8_t i = 0; i < 20; i++) { if (_rfidPollCallback) _rfidPollCallback(); delay(50U); }


  Serial.printf("MotorPin: %d\nLimit switch: %d\n", motorPin, limitSwitch);

  int h = digitalRead(limitSwitch);
  Serial.println(h);

  // run until the switch reads HIGH, without a timeout.
  while (h == LOW) {
    delay(50U);
    if (_rfidPollCallback) _rfidPollCallback(); // discard scans while dispensing
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
      runMotorSpring(motorPins[1], motorLimitSwitches[1]);
      break;
    case 3:
      runMotorSpring(motorPins[2], motorLimitSwitches[2]);
      break;
    case 4:
      runMotorSpring(motorPins[3], motorLimitSwitches[2]);
      break;
    case 5:
      runMotorSpring(motorPins[4], motorLimitSwitches[3]);
      break;
    case 6:
      runMotorSpring(motorPins[5], motorLimitSwitches[3]);
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
