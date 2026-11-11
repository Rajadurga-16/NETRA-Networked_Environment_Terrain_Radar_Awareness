/*
  NETRA - Standalone HC-SR04 ultrasonic test

  No library needed - built-in pulseIn().

  Wiring:
    HC-SR04 VCC  -> 5V
    HC-SR04 GND  -> GND
    HC-SR04 TRIG -> ESP32 GPIO4 (direct, no divider needed)
    HC-SR04 ECHO -> voltage divider -> ESP32 GPIO32
      (ECHO ---[220ohm]---+---[220ohm]---[220ohm]--- GND, junction -> GPIO32)
      DO NOT skip the divider - ECHO is a 5V signal and can damage GPIO32
      without it.

  Move your hand in front of the sensor and watch the distance change.
*/

#include "pins.h"

#define TIMEOUT_US 25000

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  Serial.println("[ULTRASONIC TEST] Started.");
}

void loop() {
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  unsigned long durationUs = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, TIMEOUT_US);

  if (durationUs == 0) {
    Serial.println("No echo (out of range, or check wiring/power).");
  } else {
    float distanceCm = (durationUs / 2.0) / 29.1;
    Serial.print("Distance: ");
    Serial.print(distanceCm);
    Serial.println(" cm");
  }

  delay(200); // HC-SR04 needs >=60ms between pings; 200ms keeps Serial Monitor readable
}
