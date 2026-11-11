#ifndef NETRA_CONFIG_H
#define NETRA_CONFIG_H

// Buzzer beep patterns (ms ON, ms OFF) per local risk indication.
// NOTE: Vehicle A's own buzzer reacts to whatever risk_level the ESP32 last
// received back from the laptop over serial (see protocol.h RX packet),
// combined with its own local ultrasonic reading - see config below.
#define BEEP_CAUTION_ON   150
#define BEEP_CAUTION_OFF  1200
#define BEEP_WARNING_ON   150
#define BEEP_WARNING_OFF  400
#define BEEP_CRITICAL_ON  120
#define BEEP_CRITICAL_OFF 120

// How often the ESP32 sends a telemetry JSON line to the laptop.
#define TELEMETRY_INTERVAL_MS 500

// How often the LCD redraws (screens still cycle every LCD_SCREEN_HOLD_MS in the .ino).
#define LCD_REFRESH_MS 400

// If no valid command comes back from the laptop within this window,
// Vehicle A firmware assumes the laptop link is down and falls back to
// showing "NO LINK" - it does NOT assume SAFE.
#define LAPTOP_LINK_TIMEOUT_MS 3000

// ---------------- Ultrasonic obstacle sensor ----------------
#define ULTRASONIC_MAX_RANGE_CM   300   // beyond this, treat as "nothing detected"
#define ULTRASONIC_TIMEOUT_US     25000 // pulseIn() timeout (~4.3m sound round-trip max)
#define ULTRASONIC_SAMPLE_MS      120   // how often to ping (HC-SR04 needs >=60ms between pings)

// ---------------- Autonomous drive / obstacle-avoidance thresholds ----------------
// These are STUDENT PROTOTYPE demo thresholds, not safety-certified values.
#define OBSTACLE_STOP_CM        20   // closer than this -> stop immediately
#define OBSTACLE_SLOW_CM        45   // closer than this -> creep forward cautiously
#define AVOID_REVERSE_MS        400  // how long to reverse before turning away
#define AVOID_TURN_MS           500  // how long to turn away from an obstacle

// If Vehicle A hasn't been told to drive (e.g. dashboard hasn't started
// AUTO patrol) it stays stopped - it never starts moving on its own without
// an explicit go-ahead. See DRIVE_ENABLE default in the .ino.

#endif
