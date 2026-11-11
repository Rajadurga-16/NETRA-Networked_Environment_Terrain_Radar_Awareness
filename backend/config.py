"""
NETRA backend configuration.
Single place for every tunable value used by the risk/collision/geofence
engines and the serial link. These thresholds are STUDENT PROTOTYPE values
for demo purposes - they are not official mine-safety standards.
"""

import os

# ---------------- Serial link to Vehicle A (ESP32) ----------------
# Windows: something like "COM5". Check Device Manager > Ports (COM & LPT).
SERIAL_PORT = os.environ.get("NETRA_SERIAL_PORT", "COM5")
SERIAL_BAUD = 115200
SERIAL_TIMEOUT_S = 1.0

# If no valid telemetry line arrives from Vehicle A within this many seconds,
# the backend marks Vehicle A OFFLINE / DEGRADED MODE - it never assumes SAFE.
VEHICLE_A_TIMEOUT_S = 3.0

# ---------------- Collision / risk thresholds (meters, m/s) ----------------
# Distance bands (used together with approach state + closing speed, not alone)
DIST_CRITICAL_M = 15
DIST_WARNING_M = 40
DIST_CAUTION_M = 80

# ---------------- Local obstacle sensor (Vehicle A's onboard HC-SR04) ----------------
# Tabletop-robot scale (meters), independent of the Vehicle A <-> Vehicle B
# distance bands above. Field is still called "radar_*" in the serial
# protocol/dashboard for compatibility - it's an ultrasonic sensor.
ULTRASONIC_VERY_CLOSE_M = 0.15  # 15 cm
ULTRASONIC_NEARBY_M = 0.40      # 40 cm

# Minimum closing speed (m/s) to count as "approaching" for risk purposes.
# Below this, relative motion is treated as noise / not meaningfully closing.
APPROACHING_CLOSING_SPEED_MS = 0.3

# Time-to-conflict thresholds (seconds) - only meaningful when closing.
TTC_CRITICAL_S = 8
TTC_WARNING_S = 20

# ---------------- Geofencing ----------------
RISK_ZONES_FILE = os.path.join(os.path.dirname(__file__), "..", "data", "risk_zones.json")

# ---------------- Vehicle B simulator ----------------
SIM_TICK_S = 1.0                 # simulator update interval
SIM_AUTO_APPROACH_SPEED_MS = 4.0  # how fast B closes distance in AUTO mode (m/s)

# ---------------- Database / event log ----------------
DB_PATH = os.path.join(os.path.dirname(__file__), "..", "data", "netra.db")
EVENT_HISTORY_LIMIT = 50  # how many recent events the dashboard shows

# ---------------- Flask ----------------
FLASK_HOST = "0.0.0.0"
FLASK_PORT = 5000
STATE_POLL_INTERVAL_MS = 1000  # how often dashboard.js polls /api/state
