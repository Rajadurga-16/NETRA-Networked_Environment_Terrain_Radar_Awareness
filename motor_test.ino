/*
  NETRA - Standalone L298N motor test

  SAFETY FIRST:
    - Put the robot chassis on a stand, box, or hold its wheels off the
      ground/table for this first test, so it can't drive off the table if
      a direction is wired backwards.
    - This sketch runs each direction for only 800ms, then stops and waits
      5 seconds before the next one - never runs continuously/unattended.

  Wiring:
    L298N +12V/motor terminal -> battery pack (+)
    L298N GND terminal        -> battery pack (-) AND ESP32 GND (common ground!)
    L298N OUT1/OUT2           -> left motor
    L298N OUT3/OUT4           -> right motor
    L298N IN1                 -> ESP32 GPIO13
    L298N IN2                 -> ESP32 GPIO14
    L298N IN3                 -> ESP32 GPIO18
    L298N IN4                 -> ESP32 GPIO19
    L298N ENA/ENB jumpers     -> LEAVE ON (full speed, no GPIO needed)
    100uF capacitor           -> across the motor-power terminal (+/-),
                                 mind polarity (long leg = +)

  If a motor spins the WRONG direction: swap that motor's two OUT wires at
  the L298N screw terminal (easier than rewiring GPIOs).
*/

#include "pins.h"

void motorsStop() {
  digitalWrite(MOTOR_LEFT_IN1, LOW);
  digitalWrite(MOTOR_LEFT_IN2, LOW);
  digitalWrite(MOTOR_RIGHT_IN3, LOW);
  digitalWrite(MOTOR_RIGHT_IN4, LOW);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(MOTOR_LEFT_IN1, OUTPUT);
  pinMode(MOTOR_LEFT_IN2, OUTPUT);
  pinMode(MOTOR_RIGHT_IN3, OUTPUT);
  pinMode(MOTOR_RIGHT_IN4, OUTPUT);
  motorsStop();
  Serial.println("[MOTOR TEST] Starting in 3 seconds - make sure wheels are off the ground!");
  delay(3000);
}

void loop() {
  Serial.println("FORWARD");
  digitalWrite(MOTOR_LEFT_IN1, HIGH);  digitalWrite(MOTOR_LEFT_IN2, LOW);
  digitalWrite(MOTOR_RIGHT_IN3, HIGH); digitalWrite(MOTOR_RIGHT_IN4, LOW);
  delay(800);
  motorsStop();
  delay(5000);

  Serial.println("REVERSE");
  digitalWrite(MOTOR_LEFT_IN1, LOW);  digitalWrite(MOTOR_LEFT_IN2, HIGH);
  digitalWrite(MOTOR_RIGHT_IN3, LOW); digitalWrite(MOTOR_RIGHT_IN4, HIGH);
  delay(800);
  motorsStop();
  delay(5000);

  Serial.println("TURN RIGHT (left wheel fwd, right wheel rev)");
  digitalWrite(MOTOR_LEFT_IN1, HIGH); digitalWrite(MOTOR_LEFT_IN2, LOW);
  digitalWrite(MOTOR_RIGHT_IN3, LOW); digitalWrite(MOTOR_RIGHT_IN4, HIGH);
  delay(800);
  motorsStop();
  delay(5000);
}
