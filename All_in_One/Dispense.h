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
constexpr int motorLimitSwitches[4] = { 
  GPIO_NUM_21, GPIO_NUM_19, 
  GPIO_NUM_18, GPIO_NUM_13 
};

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
