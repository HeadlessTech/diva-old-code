// Dispense.h
#pragma once

// Callback registered by the main sketch so the motor spin loop can call
// mqtt.loop() without introducing a circular include between Dispense.h and MQTT.h.
static void (*_mqttLoopCallback)() = nullptr;
inline void setMqttLoopCallback(void (*fn)()) { _mqttLoopCallback = fn; }

constexpr int motorPins[8] = {
  GPIO_NUM_23, GPIO_NUM_22,
  GPIO_NUM_4, GPIO_NUM_2,
  GPIO_NUM_32, GPIO_NUM_33,
  GPIO_NUM_26, GPIO_NUM_27,
};
#if IS_TWO_GROUP
// Two-group 160 machine: 6 limit switches
// Col 1 (BELT):  motor1(23) + motor2(22) -> limit1(21) + limit2(19)
// Col 2 (BELT):  motor3(4)  + motor4(2)  -> limit3(18) + limit4(13)
// Col 3 (PANTY): motor5(32) -> limit5(34)
// Col 4 (PANTY): motor6(33) -> limit5(34)
// Col 5 (PANTY): motor7(26) -> limit6(35)
// Col 6 (PANTY): motor8(27) -> limit6(35)
// GPIO 34 and 35 have no internal pull-up (INPUT_PULLUP does nothing there),
// so the board must have pull-up resistors on them. GPIO 35 is the cash
// acceptor's input on standard machines, so this machine has no cash.
constexpr int motorLimitSwitches[6] = {
  GPIO_NUM_21, GPIO_NUM_19,
  GPIO_NUM_18, GPIO_NUM_13,
  GPIO_NUM_34, GPIO_NUM_35,
};
#else
// Standard machines: up to 60 (1-3 columns), each column has its own limit
// switch; 120 and 160 (6 or 8 columns), columns share them in pairs
// (1-2, 3-4, 5-6, 7-8). See dispenseProcess().
constexpr int motorLimitSwitches[4] = { 
  GPIO_NUM_21, GPIO_NUM_19, 
  GPIO_NUM_18, GPIO_NUM_13 
};
#endif

#if IS_TWO_GROUP

// NORMAL dispense: single motor, single limit switch (with timeout)
void dispenseNormal(int m, int l) {
  digitalWrite(m, HIGH);
  Serial.printf("Motor Pin: %d\nLimit switch: %d\n", m, l);

  delay(1000U); // wait for motor to get out of rotation sensor first

  int h = digitalRead(l);
  uint32_t dispenseStartMillis = millis();

  while (h == LOW && millis() - dispenseStartMillis < 10000U) {
    delay(25U);
    if (_mqttLoopCallback) _mqttLoopCallback();
    h = digitalRead(l);
    if (h != LOW) {
      digitalWrite(m, LOW);
    }
    Serial.print(h);
  }

  // timeout safety
  digitalWrite(m, LOW);
  Serial.println("");
}

// LARGE dispense: dual motor, dual limit switch (with timeout)
void dispenseLarge(int m1, int m2, int l1, int l2) {
  digitalWrite(m1, HIGH);
  digitalWrite(m2, HIGH);
  delay(1000); // wait for motors to get out of rotation sensor first

  Serial.printf("Motor pins: %d, %d\nLimit switches: %d, %d\n", m1, m2, l1, l2);

  int h0 = digitalRead(l1);
  int h1 = digitalRead(l2);
  uint32_t dispenseStartMillis = millis();

  while ((h0 == LOW || h1 == LOW) && (millis() - dispenseStartMillis < 10000U)) {
    delay(25U);
    if (_mqttLoopCallback) _mqttLoopCallback();
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

  // timeout safety
  digitalWrite(m1, LOW);
  digitalWrite(m2, LOW);
  Serial.println("");
}

void dispenseProcess(int col) {
  Serial.println("Dispensing... ");
  // 160-cap dual-group machine: cols 1-2 are LARGE (dual motor), cols 3-6 are NORMAL (single motor)
  switch (col) {
  case 1: dispenseLarge(motorPins[0], motorPins[1], motorLimitSwitches[0], motorLimitSwitches[1]); break;
  case 2: dispenseLarge(motorPins[2], motorPins[3], motorLimitSwitches[2], motorLimitSwitches[3]); break;
  case 3: dispenseNormal(motorPins[4], motorLimitSwitches[4]); break;
  case 4: pinMode(motorPins[5], OUTPUT); dispenseNormal(motorPins[5], motorLimitSwitches[4]); break;  // shares limit5 with col 3
  case 5: dispenseNormal(motorPins[6], motorLimitSwitches[5]); break;
  case 6: dispenseNormal(motorPins[7], motorLimitSwitches[5]); break;  // shares limit6 with col 5
  default: break;
  }

  Serial.println("Dispense Done!");
}

#else  // standard machines

void runMotorSpring(int motorPin, int limitSwitch) {  // spring-type dispense chamber
  delay(20U);
  digitalWrite(motorPin, HIGH);
  // Wait for the motor to start turning before polling the limit switch.
  // Use two short loops so we keep calling mqtt.loop() throughout.
  for (uint8_t i = 0; i < 10; i++) { if (_mqttLoopCallback) _mqttLoopCallback(); delay(50U); }

  Serial.printf("MotorPin: %d\nLimit switch: %d\n", motorPin, limitSwitch);

  int h = digitalRead(limitSwitch);
  Serial.println(h);

  // Safety timeout: if the limit switch never triggers within 8 s, force-stop the motor.
  static const unsigned long MOTOR_TIMEOUT_MS = 8000UL;
  unsigned long motorStart = millis();
  bool timedOut = false;
  while (h == LOW) {
    if (millis() - motorStart > MOTOR_TIMEOUT_MS) {
      timedOut = true;
      break;
    }
    delay(25U);
    if (_mqttLoopCallback) _mqttLoopCallback(); // keep MQTT/SSL alive during spin
    h = digitalRead(limitSwitch);
    Serial.print(h);
  }
  // Always drop the pin, normal stop or timeout. Belt-and-suspenders: re-assert
  // OUTPUT in case the GPIO got flipped back to INPUT by some peripheral driver.
  // pinMode(motorPin, OUTPUT);
  digitalWrite(motorPin, LOW);
  if (timedOut) {
    Serial.printf("\nmotor %d TIMEOUT (no limit-switch edge in %lu ms) - forced stop",
                  motorPin, MOTOR_TIMEOUT_MS);
  } else {
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
      // GPIO 33 was switched to an analog input before every TLS connection
      // (SSLClient read it as A5 for randomness; fixed, see SSL.h). Setting
      // OUTPUT again before driving it is kept as a harmless safeguard.
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

#endif  // IS_TWO_GROUP
