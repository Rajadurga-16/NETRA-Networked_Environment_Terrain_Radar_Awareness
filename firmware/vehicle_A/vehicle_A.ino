/*
  NETRA - Vehicle A Firmware (v3: HC-SR04 ultrasonic + L298N motor driver)

  Required Arduino Library Manager installs (EXACT names):
    - "LiquidCrystal I2C"      by Frank de Brabander (16x2 I2C LCD driver)
    - "TinyGPSPlus"            by Mikal Hart
    - "ArduinoJson"            by Benoit Blanchon  (use version 6.x)
  (No library needed for HC-SR04 or L298N - both are plain digitalWrite/
  pulseIn/GPIO control, built into the core.)

  Board: Tools > Board > ESP32 Arduino > ESP32 Dev Module

  Files in this sketch folder: vehicle_A.ino, pins.h, config.h, protocol.h
  (Arduino IDE will show them as tabs - that's correct, don't move them.)

  SAFETY: motors stay stopped until the laptop explicitly sends
  "drive_enabled": true (see protocol.h). Even then, the ultrasonic sensor
  can force an immediate local stop regardless of what the laptop says -
  local obstacle safety never waits on the serial link.
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGPSPlus.h>
#include <ArduinoJson.h>
#include "pins.h"
#include "config.h"
#include "protocol.h"

// ---------------- 16x2 LCD ----------------
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
bool lcdReady = false;

uint8_t lcdScreen = 0;
#define LCD_SCREEN_COUNT 3
#define LCD_SCREEN_HOLD_MS 2000
unsigned long lastLcdScreenSwitch = 0;

// ---------------- GPS ----------------
TinyGPSPlus gps;
HardwareSerial GPSSerial(1);   // UART1

// ---------------- HC-SR04 ultrasonic ----------------
bool ultrasonicTargetPresent = false;
float ultrasonicDistanceCm = -1;
unsigned long lastUltrasonicSampleMillis = 0;
unsigned long lastUltrasonicValidMillis = 0;
#define ULTRASONIC_ONLINE_TIMEOUT_MS 1000  // no valid echo in this window -> OFFLINE

// ---------------- Motors ----------------
bool driveEnabledByLaptop = false;  // safety gate - starts false, laptop must opt in
bool currentlyDriving = false;

// ---------------- STATE ----------------
uint32_t seqNum = 0;
unsigned long lastTelemetryMillis = 0;
unsigned long lastLcdMillis = 0;
unsigned long lastCommandMillis = 0;   // last time we heard from laptop
unsigned long lastBuzzToggle = 0;
bool buzzOn = false;

String currentRiskLevel = "SAFE";
bool currentZoneAlert = false;
String currentMessage = "Waiting for laptop link...";
bool laptopLinked = false;

// GPS simulation fallback (indoor testing, clearly labeled, never presented as live)
bool gpsSimMode = false;
double simLat = 18.00010;
double simLon = 82.00010;
#define GPS_FIX_TIMEOUT_MS 8000
unsigned long bootMillis = 0;

// ---------------- Buzzer / LEDs ----------------

void beep(int ms) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

void updateBuzzerAndLeds() {
  unsigned long now = millis();

  if (currentRiskLevel == "SAFE") {
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_GREEN_PIN, HIGH);
    digitalWrite(LED_RED_PIN, LOW);
    return;
  }

  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_RED_PIN, HIGH);

  int onMs, offMs;
  if (currentRiskLevel == "CAUTION") { onMs = BEEP_CAUTION_ON; offMs = BEEP_CAUTION_OFF; }
  else if (currentRiskLevel == "WARNING") { onMs = BEEP_WARNING_ON; offMs = BEEP_WARNING_OFF; }
  else { onMs = BEEP_CRITICAL_ON; offMs = BEEP_CRITICAL_OFF; } // CRITICAL

  unsigned long interval = buzzOn ? onMs : offMs;
  if (now - lastBuzzToggle >= interval) {
    buzzOn = !buzzOn;
    lastBuzzToggle = now;
    digitalWrite(BUZZER_PIN, buzzOn ? HIGH : LOW);
  }
}

// ---------------- GPS ----------------

void readGPS() {
  while (GPSSerial.available() > 0) {
    gps.encode(GPSSerial.read());
  }
}

bool gpsHasFix() {
  return gps.location.isValid() && gps.location.age() < 5000;
}

// ---------------- HC-SR04 ultrasonic ----------------

void readUltrasonic() {
  unsigned long now = millis();
  if (now - lastUltrasonicSampleMillis < ULTRASONIC_SAMPLE_MS) return;
  lastUltrasonicSampleMillis = now;

  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  unsigned long durationUs = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, ULTRASONIC_TIMEOUT_US);

  if (durationUs == 0) {
    // Timed out - genuinely no echo at all (sensor disconnected, or nothing
    // within its max physical range). Mark both "no target" and let
    // ultrasonicOnline() age out on its own - don't touch lastUltrasonicValidMillis.
    ultrasonicTargetPresent = false;
    return;
  }

  // Got a real echo -> sensor is alive, regardless of what the distance is.
  lastUltrasonicValidMillis = now;

  float distanceCm = (durationUs / 2.0) / 29.1; // speed of sound ~343m/s -> 29.1 us/cm round trip
  if (distanceCm <= 0 || distanceCm > ULTRASONIC_MAX_RANGE_CM) {
    ultrasonicTargetPresent = false; // in range of the sensor's physics, but beyond our "care about it" radius
    return;
  }

  ultrasonicDistanceCm = distanceCm;
  ultrasonicTargetPresent = true;
}

bool ultrasonicOnline() {
  if (lastUltrasonicValidMillis == 0) return false; // never had a valid reading yet
  return (millis() - lastUltrasonicValidMillis) < ULTRASONIC_ONLINE_TIMEOUT_MS;
}

// ---------------- Motors (L298N) ----------------

void motorsStop() {
  digitalWrite(MOTOR_LEFT_IN1, LOW);
  digitalWrite(MOTOR_LEFT_IN2, LOW);
  digitalWrite(MOTOR_RIGHT_IN3, LOW);
  digitalWrite(MOTOR_RIGHT_IN4, LOW);
  currentlyDriving = false;
}

void motorsForward() {
  digitalWrite(MOTOR_LEFT_IN1, HIGH);
  digitalWrite(MOTOR_LEFT_IN2, LOW);
  digitalWrite(MOTOR_RIGHT_IN3, HIGH);
  digitalWrite(MOTOR_RIGHT_IN4, LOW);
  currentlyDriving = true;
}

void motorsReverse() {
  digitalWrite(MOTOR_LEFT_IN1, LOW);
  digitalWrite(MOTOR_LEFT_IN2, HIGH);
  digitalWrite(MOTOR_RIGHT_IN3, LOW);
  digitalWrite(MOTOR_RIGHT_IN4, HIGH);
  currentlyDriving = true;
}

void motorsTurnRight() {
  // Left wheel forward, right wheel reverse -> pivot right (turn away from
  // an obstacle detected ahead).
  digitalWrite(MOTOR_LEFT_IN1, HIGH);
  digitalWrite(MOTOR_LEFT_IN2, LOW);
  digitalWrite(MOTOR_RIGHT_IN3, LOW);
  digitalWrite(MOTOR_RIGHT_IN4, HIGH);
  currentlyDriving = true;
}

// Simple obstacle-avoidance state machine. Called every loop() iteration.
// This is LOCAL safety logic - it runs regardless of the laptop link, and
// driveEnabledByLaptop is checked first so the robot never moves unless the
// dashboard has explicitly said it's allowed to.
enum DriveState { DRIVE_STOPPED, DRIVE_FORWARD, DRIVE_REVERSING, DRIVE_TURNING };
DriveState driveState = DRIVE_STOPPED;
unsigned long driveStateEnteredMillis = 0;

void updateDrive() {
  unsigned long now = millis();

  if (!driveEnabledByLaptop) {
    if (driveState != DRIVE_STOPPED) {
      motorsStop();
      driveState = DRIVE_STOPPED;
    }
    return;
  }

  bool obstacleClose = ultrasonicOnline() && ultrasonicTargetPresent
                        && ultrasonicDistanceCm <= OBSTACLE_STOP_CM;

  switch (driveState) {
    case DRIVE_STOPPED:
      if (!obstacleClose) {
        motorsForward();
        driveState = DRIVE_FORWARD;
        driveStateEnteredMillis = now;
      }
      break;

    case DRIVE_FORWARD:
      if (obstacleClose) {
        motorsReverse();
        driveState = DRIVE_REVERSING;
        driveStateEnteredMillis = now;
      }
      break;

    case DRIVE_REVERSING:
      if (now - driveStateEnteredMillis >= AVOID_REVERSE_MS) {
        motorsTurnRight();
        driveState = DRIVE_TURNING;
        driveStateEnteredMillis = now;
      }
      break;

    case DRIVE_TURNING:
      if (now - driveStateEnteredMillis >= AVOID_TURN_MS) {
        motorsStop();
        driveState = DRIVE_STOPPED; // re-evaluate from STOPPED next loop
        driveStateEnteredMillis = now;
      }
      break;
  }
}

// ---------------- Serial JSON to/from laptop ----------------

void sendTelemetry() {
  StaticJsonDocument<384> doc;
  doc["vehicle_id"] = "A";

  double lat, lon;
  const char* gpsStatus;
  if (gpsHasFix()) {
    lat = gps.location.lat();
    lon = gps.location.lng();
    gpsStatus = "OK";
    gpsSimMode = false;
  } else if (millis() - bootMillis > GPS_FIX_TIMEOUT_MS) {
    gpsSimMode = true;
    lat = simLat;
    lon = simLon;
    gpsStatus = "SIM";
  } else {
    lat = 0; lon = 0;
    gpsStatus = "NO_FIX";
  }

  doc["latitude"] = lat;
  doc["longitude"] = lon;
  doc["speed"] = gpsHasFix() ? gps.speed.kmph() : 0.0;
  doc["heading"] = gpsHasFix() ? gps.course.deg() : 0.0;
  doc["gps_status"] = gpsStatus;
  doc["radar_status"] = ultrasonicOnline() ? "OK" : "OFFLINE";
  doc["radar_target"] = ultrasonicOnline() && ultrasonicTargetPresent;
  doc["radar_distance"] = (ultrasonicOnline() && ultrasonicTargetPresent) ? (ultrasonicDistanceCm / 100.0) : -1;
  doc["driving"] = currentlyDriving;
  doc["seq"] = seqNum++;
  doc["timestamp"] = millis();

  serializeJson(doc, Serial);
  Serial.println();
}

void readCommandsFromLaptop() {
  static String lineBuf = "";
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n') {
      if (lineBuf.length() > 0) {
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, lineBuf);
        if (!err) {
          if (doc.containsKey("risk_level")) currentRiskLevel = doc["risk_level"].as<String>();
          if (doc.containsKey("zone_alert")) currentZoneAlert = doc["zone_alert"];
          if (doc.containsKey("message")) currentMessage = doc["message"].as<String>();
          if (doc.containsKey("drive_enabled")) driveEnabledByLaptop = doc["drive_enabled"];
          lastCommandMillis = millis();
          laptopLinked = true;
        }
      }
      lineBuf = "";
    } else if (c != '\r') {
      lineBuf += c;
      if (lineBuf.length() > 300) lineBuf = "";
    }
  }

  if (millis() - lastCommandMillis > LAPTOP_LINK_TIMEOUT_MS) {
    laptopLinked = false;
    driveEnabledByLaptop = false; // laptop link lost -> force-stop, never assume OK to drive
  }
}

// ---------------- 16x2 LCD ----------------

void lcdPrintLine(uint8_t row, String text) {
  while (text.length() < LCD_COLS) text += ' ';
  if (text.length() > LCD_COLS) text = text.substring(0, LCD_COLS);
  lcd.setCursor(0, row);
  lcd.print(text);
}

void updateLcd() {
  if (!lcdReady) return;

  unsigned long now = millis();
  if (now - lastLcdScreenSwitch >= LCD_SCREEN_HOLD_MS) {
    lastLcdScreenSwitch = now;
    lcdScreen = (lcdScreen + 1) % LCD_SCREEN_COUNT;
  }

  if (lcdScreen == 0) {
    lcdPrintLine(0, "NETRA A  " + String(laptopLinked ? "LINK" : "NOLNK"));
    String gpsTxt = gpsHasFix() ? "OK" : (gpsSimMode ? "SIM" : "NOFIX");
    lcdPrintLine(1, "G:" + gpsTxt + " U:" + String(ultrasonicOnline() ? "OK" : "OFF"));

  } else if (lcdScreen == 1) {
    lcdPrintLine(0, "Obstacle:");
    if (ultrasonicOnline() && ultrasonicTargetPresent) {
      lcdPrintLine(1, String(ultrasonicDistanceCm, 0) + "cm " + (currentlyDriving ? "DRV" : ""));
    } else {
      lcdPrintLine(1, "none detected");
    }

  } else {
    if (!laptopLinked) {
      lcdPrintLine(0, "RISK:");
      lcdPrintLine(1, "NO LINK");
    } else {
      lcdPrintLine(0, "RISK:" + (currentZoneAlert ? String(" ZONE!") : String("")));
      lcdPrintLine(1, currentRiskLevel + (driveEnabledByLaptop ? " DRV" : " STOP"));
    }
  }
}

// ---------------- Setup / Loop ----------------

void setup() {
  Serial.begin(115200);
  delay(300);
  bootMillis = millis();

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, LOW);

  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  pinMode(MOTOR_LEFT_IN1, OUTPUT);
  pinMode(MOTOR_LEFT_IN2, OUTPUT);
  pinMode(MOTOR_RIGHT_IN3, OUTPUT);
  pinMode(MOTOR_RIGHT_IN4, OUTPUT);
  motorsStop(); // motors OFF at boot, always - safety default

  Wire.begin(LCD_SDA, LCD_SCL);
  Wire.beginTransmission(LCD_I2C_ADDR);
  lcdReady = (Wire.endTransmission() == 0);

  if (lcdReady) {
    lcd.init();
    lcd.backlight();
    lcdPrintLine(0, "NETRA A");
    lcdPrintLine(1, "Booting...");
  } else {
    Serial.println("[NETRA] ERROR: LCD not found at I2C address 0x27.");
    Serial.println("[NETRA] Check wiring (SDA=GPIO21, SCL=GPIO22) and try 0x3F, or run i2c_scanner.ino.");
  }

  GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  beep(120); delay(100); beep(120); // 2 beeps = firmware alive

  lastUltrasonicValidMillis = 0; // starts as OFFLINE until first valid echo
  lastCommandMillis = 0;         // starts as NO LINK until laptop talks to us
}

void loop() {
  readGPS();
  readUltrasonic();
  readCommandsFromLaptop();
  updateDrive();

  unsigned long now = millis();

  if (now - lastTelemetryMillis >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMillis = now;
    sendTelemetry();
  }

  if (now - lastLcdMillis >= LCD_REFRESH_MS) {
    lastLcdMillis = now;
    updateLcd();
  }

  updateBuzzerAndLeds();
}
