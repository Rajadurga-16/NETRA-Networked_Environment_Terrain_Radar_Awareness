#ifndef NETRA_PROTOCOL_H
#define NETRA_PROTOCOL_H

/*
  NETRA SERIAL PROTOCOL (USB, one JSON object per line, newline-terminated)

  ================================
  ESP32 -> LAPTOP  (telemetry, sent every TELEMETRY_INTERVAL_MS)
  ================================
  {
    "vehicle_id":"A",
    "latitude":18.0001,
    "longitude":82.0001,
    "speed":12.5,
    "heading":90,
    "gps_status":"OK",            // "OK" | "NO_FIX" | "SIM"
    "radar_status":"OK",          // "OK" | "OFFLINE" - field name kept for backend
                                   // compatibility; this is now the HC-SR04
                                   // ultrasonic sensor, not a radar.
    "radar_target":true,          // true if an object is within ULTRASONIC_MAX_RANGE_CM
    "radar_distance":0.42,        // meters (ultrasonic cm reading / 100)
    "driving":false,              // true if motors are currently commanded to move
    "seq":123,
    "timestamp":123456            // millis() since boot
  }

  ================================
  LAPTOP -> ESP32  (risk feedback + drive control, sent whenever it changes,
  at most every 500ms)
  ================================
  {
    "risk_level":"WARNING",       // "SAFE" | "CAUTION" | "WARNING" | "CRITICAL"
    "zone_alert":false,           // true if inside a high-risk geofence
    "message":"B: 42m APPROACHING",
    "drive_enabled":false         // true = allowed to drive; false = must stay
                                   // stopped no matter what. Defaults false on
                                   // the ESP32 until the laptop explicitly says
                                   // true - the robot never moves on its own
                                   // at power-on.
  }

  Both directions: exactly one JSON object per line. Malformed lines must be
  skipped, never crash the parser on either side.
*/

#endif
