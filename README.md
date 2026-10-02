<p align="center">
  <img
    src="assets/SIHlogo.png"
    alt="Smart India Hackathon"
    width="220"
  />
</p>

# NETRA - Networked Environment, Terrain and Radar Awareness
* Networked → vehicle-to-vehicle / vehicle-to-system communication
* Environment → fog and low-visibility conditions
* Terrain → blind curves, risky road sections, mine-road conditions
* Radar → detects nearby objects/vehicles without depending entirely on visibility
* Awareness → gives warnings to the driver/operator
### SIH 2026 | PS 26007 | NMDC — Safe HEMM Movement in Fog & Low Visibility
Safe and Efficient Operation of Mine Vehicles in Fog and Low-Visibility Conditions in Open Cast Iron Ore Mines.
---

NETRA is a prototype mine-vehicle safety and risk-awareness system. It combines **ultrasonic obstacle sensing, GPS positioning, geofenced risk zones, a deterministic risk-fusion engine, and a multilingual voice-guided dashboard** to warn operators of potential vehicle conflicts or hazardous zones — and, on the physical prototype, to autonomously stop/avoid obstacles locally.

The prototype uses **one physically-driving vehicle (Vehicle A)** and a **software-simulated second vehicle (Vehicle B)** to demonstrate vehicle-to-vehicle (V2V) risk detection without requiring two complete physical builds.

**Student prototype — not a certified mine-safety system.**

---

## 🚨 Problem

Open-cast mining vehicles (HEMM, dumpers) operate on roads where visibility drops due to fog, dust, blind curves, large vehicle blind spots, and difficult terrain. Depending on visual observation alone makes it hard for an operator to recognize nearby vehicles or hazards early enough.

NETRA adds a **non-optical layer of driver awareness** on top of normal visual operation.

---

## 💡 What NETRA actually is

The project went through a hardware correction after initial parts sourcing. Here's what changed, and why it matters for anyone reading the code:

| Originally planned | What's actually built | Why |
|---|---|---|
| HLK-LD2450 radar (UART) | **HC-SR04 ultrasonic** (trigger/echo) | Simpler, cheaper, no UART — needs a voltage divider instead |
| 0.96" SSD1306 OLED | **16x2 I2C LCD** | Different display driven differently — cycles 3 short screens instead of one richer one |
| NEO-M8N GPS | **NEO-6M GPS** | Same NMEA protocol, zero code difference |
| Stationary Vehicle A | **2WD chassis + L298N motor driver + 2×18650 battery** | Vehicle A physically drives and does local autonomous obstacle avoidance — a capability the original plan didn't have |
| Vehicle B | Still fully **software-simulated** (MANUAL or AUTO-approach mode) | No second physical build needed to demonstrate V2V risk logic |

The serial protocol's JSON fields are still named `radar_status` / `radar_target` / `radar_distance` for backend compatibility — read these as **"ultrasonic"** everywhere in the code. This is a known, intentional naming debt, not a claim that radar hardware exists.

---

## 🏗️ System Architecture

```text
                 ┌─────────────────────────┐
                 │   PHYSICAL VEHICLE A    │
                 │                         │
                 │  HC-SR04 Ultrasonic     │
                 │  NEO-6M GPS             │
                 │  16x2 I2C LCD           │
                 │  Buzzer / Red+Green LED │
                 │  L298N + 2x DC Motors   │
                 │        ESP32            │
                 └────────────┬────────────┘
                              │ USB Serial (JSON lines, both directions)
                              ▼
                 ┌─────────────────────────┐
                 │      NETRA BACKEND      │
                 │     (Flask, Python)     │
                 │                         │
                 │ Serial Gateway          │
                 │ Vehicle B Simulator     │
                 │ Collision Engine        │
                 │ Risk Engine             │
                 │ Geofence Engine         │
                 │ Event Logger + SQLite DB│
                 └────────────┬────────────┘
                              │
                              ▼
                 ┌─────────────────────────┐
                 │       DASHBOARD         │
                 │                         │
                 │ Live Vehicle Status     │
                 │ Digital Map (map.js)    │
                 │ Risk Level + Reasons    │
                 │ Voice Alerts (EN/HI/TA) │
                 │ Risk Zones              │
                 │ Event Log               │
                 └─────────────────────────┘

                 SOFTWARE VEHICLE B
                         │
                         ▼
               Vehicle B Simulator (MANUAL or AUTO-approach)
                         │
                         ▼
                  Collision Engine (haversine distance + closing speed + TTC)
                         │
                         ▼
                   Risk Engine (weighted score 0–100)
```

---

## 🔍 Key Features

### 1. Ultrasonic Obstacle Sensing + Autonomous Local Avoidance
The HC-SR04 gives Vehicle A a non-optical close-range sensing layer. When driving is enabled and an obstacle is detected within `OBSTACLE_STOP_CM`, the **ESP32 itself** stops, reverses, turns, and resumes — entirely locally, with no dependency on the laptop link.

### 2. GPS-Based Vehicle Position
NEO-6M GPS provides Vehicle A's live lat/lon, used for the digital map, geofenced risk zones, and distance-to-Vehicle-B calculation.

### 3. Vehicle-to-Vehicle Risk Analysis
- **Vehicle A → real hardware**
- **Vehicle B → software simulation** (MANUAL: set lat/lon/speed/heading by hand via dashboard; AUTO: gradually closes distance toward Vehicle A to demo the full SAFE→CRITICAL escalation hands-free)

The backend computes great-circle (haversine) distance between A and B every tick, tracks whether they're APPROACHING / RECEDING / STATIONARY from the actual rate of change in distance, and estimates time-to-conflict (TTC) when closing.

### 4. Four-Level Risk Classification (weighted, explainable)
| Risk Level | Score | Meaning |
|---|---|---|
| 🟢 SAFE | 0–19 | No immediate conflict |
| 🟡 CAUTION | 20–44 | Increased attention required |
| 🟠 WARNING | 45–69 | Potential conflict — reduce speed |
| 🔴 CRITICAL | 70–100 | High-risk — stop |

The score is a deterministic, explainable weighted sum (V2V distance band + closing/TTC + local ultrasonic obstacle + geofence severity) — **not** a black-box ML model. Every score ships with a `reasons[]` list.

### 5. Geofenced Risk Zones
`data/risk_zones.json` defines circular hazard zones (center lat/lon + radius) independent of application code:
```json
{
  "zone_id": "ZONE_01",
  "name": "Blind Curve - Haul Road 3",
  "latitude": 18.00010,
  "longitude": 82.00010,
  "radius_m": 50,
  "hazard_type": "BLIND_CURVE",
  "severity": "HIGH",
  "recommended_speed": 15
}
```

### 6. Three-Layer Driving Safety Model
Any one of these independently stops the motors:
1. **Dashboard toggle** — operator permission, off by default every backend restart.
2. **Backend override** — refuses to forward `drive_enabled=true` whenever Vehicle A is offline or risk is CRITICAL/UNKNOWN, regardless of the toggle.
3. **ESP32 local cutoff** — stops motors the instant its own ultrasonic sees an obstacle, or the laptop link times out — independent of the backend entirely.

### 7. Degraded Mode (never silently "safe")
If Vehicle A's serial link drops, the dashboard shows `DEGRADED MODE` / `VEHICLE A OFFLINE`, driving is force-refused, and risk is reported as `UNKNOWN` — never defaulted to SAFE.

### 8. Multilingual Voice Guidance
The dashboard speaks alerts in **English, Hindi, or Tamil** via the browser's built-in Web Speech API (`dashboard/static/js/voice.js`) — no extra hardware, no cloud dependency. Triggers: risk level changes, zone entry, Vehicle A offline, close-range obstacles. Voice availability depends on what the OS/browser has installed; the on-screen caption is always correct regardless.

### 9. Digital Mine Map + Event Log
Live map (`map.js`) shows Vehicle A (real) and Vehicle B (simulated) positions, risk zones, and vehicle trail. Event log persists to SQLite (`data/netra.db`) and is queryable via `/api/events`.

---

## 🧰 Hardware (as actually built)

| Component | Qty | Notes |
|---|---|---|
| ESP32 DevKit V1 | 1 | Main controller |
| HC-SR04 ultrasonic sensor | 1 | Obstacle detection (needs voltage divider — see §1) |
| NEO-6M GPS module | 1 | UART1 |
| 16x2 I2C LCD (HD44780-compatible) | 1 | Default I2C addr 0x27, some boards 0x3F |
| L298N motor driver module | 1 | The red board with screw terminals |
| DC geared motors + wheels (2WD chassis) | 2 | |
| 18650 Li-ion cells + holder | 2 | Motor power — separate domain from ESP32 |
| Active buzzer | 1 | |
| Red LED + Green LED | 1 each | |
| 220Ω resistor | ≥3 | 2 for LEDs, 3 for the HC-SR04 voltage divider |
| 100µF electrolytic capacitor | 1 | Across L298N motor-power terminal — absorbs motor-start voltage spikes |
| Breadboard | 1 | |
| Jumper wires | ~30 | |
| USB cable (data-capable) | 1 | ESP32 ↔ laptop |

---

## 💻 Software Stack

**Embedded:** ESP32, Arduino IDE 2.x, C/C++ — libraries: `LiquidCrystal I2C`, `TinyGPSPlus`, `ArduinoJson` (v6.x). No library needed for HC-SR04/L298N — plain `digitalWrite`/`pulseIn`.

**Backend:** Python, Flask, pyserial, SQLite (stdlib `sqlite3`)

**Frontend:** HTML, CSS, JavaScript — Web Speech API for voice, no frameworks

---

## 📁 Project Structure (actual)

```text
NETRA/
├── firmware/
│   ├── vehicle_A/        # combined firmware — full safety logic
│   ├── gps_test/         # standalone GPS test
│   ├── ultrasonic_test/  # standalone HC-SR04 test
│   ├── motor_test/       # standalone L298N direction test (wheels off ground first!)
│   ├── i2c_scanner/      # LCD I2C address finder
│   └── common/           # reference copies of pins.h / config.h / protocol.h
│
├── backend/
│   ├── app.py              # Flask app + background decision loop
│   ├── config.py           # all tunable thresholds in one place
│   ├── serial_gateway.py   # reads/writes JSON lines to the ESP32
│   ├── vehicle_simulator.py# Vehicle B (MANUAL / AUTO modes)
│   ├── collision_engine.py # haversine distance, closing speed, TTC
│   ├── risk_engine.py      # weighted score -> SAFE/CAUTION/WARNING/CRITICAL
│   ├── geofence_engine.py  # circular risk-zone checks
│   ├── database.py         # SQLite init/access
│   └── event_logger.py     # event history (DB-backed)
│
├── dashboard/
│   ├── templates/index.html
│   └── static/
│       ├── css/style.css
│       └── js/
│           ├── dashboard.js   # polling + rendering
│           ├── map.js         # digital mine map
│           └── voice.js       # EN/HI/TA voice alerts
│
├── data/
│   ├── risk_zones.json   # geofence definitions
│   └── netra.db          # created at runtime (SQLite)
│
├── tests/
│   ├── test_collision.py
│   ├── test_risk.py
│   └── test_geofence.py
│
├── requirements.txt
├── run.py                 # entry point: python run.py
└── README.md
```

---

## 📡 Serial Protocol (USB, JSON Lines)

**ESP32 → laptop** (every `TELEMETRY_INTERVAL_MS`):
```json
{
  "vehicle_id": "A",
  "latitude": 18.0001,
  "longitude": 82.0001,
  "speed": 12.5,
  "heading": 90,
  "gps_status": "OK",
  "radar_status": "OK",
  "radar_target": true,
  "radar_distance": 0.42,
  "driving": false,
  "seq": 123,
  "timestamp": 123456
}
```
*(`radar_*` fields = ultrasonic sensor; name kept for backend compatibility — see §"What NETRA actually is")*

**Laptop → ESP32** (whenever risk/drive state changes, max every 500ms):
```json
{
  "risk_level": "WARNING",
  "zone_alert": false,
  "message": "B: 42m APPROACHING",
  "drive_enabled": false
}
```

Both directions: one JSON object per line. Malformed lines are skipped, never crash the parser.

---

## 🧪 Testing

21 test cases (T01–T21) covering boot, LCD, buzzer, LEDs, GPS fix/no-fix, ultrasonic detection/unavailability, motor directions, drive-safety defaults, local obstacle stop, link-loss stop, JSON telemetry, Vehicle B simulation, risk escalation, zone entry, voice guidance, and full end-to-end demo. See `README` test table / `tests/` for pytest coverage of the collision/risk/geofence engines (no hardware required):
```bash
python -m pytest tests/
```
**Record actual results when testing — never invent them.**

---

## 📈 Future Development

Multiple physical vehicles · industrial-grade mining radar · thermal cameras · RTK/DGPS · mine-wide V2V/V2I communication · roadside hazard beacons · centralized control-room monitoring · historical risk analysis · driver behavior analysis · mine-specific terrain models · fleet-management integration · industrial enclosure/certification.

These are future directions, not capabilities of the current prototype.

---

## ⚠️ Prototype Limitations

- HC-SR04 is a short-range (~2cm–4m) ultrasonic sensor, **not** mining-grade radar.
- NEO-6M is prototype-grade GPS, **not** RTK/DGPS/survey-grade.
- The risk engine is a deterministic, explainable rule set — **not** a trained ML model.
- Vehicle A is a tabletop 2WD demo chassis, **not** haul-truck scale — it demonstrates system architecture and concept, not a certified mine-safety system.
- Not a replacement for trained mine operators or existing mine safety procedures.
- Not a guaranteed collision-prevention system.

---

## 👥 Team

* **Project:** NETRA — Networked Environment, Terrain and Radar Awareness
* **Hackathon:** Smart India Hackathon 2026
* **Problem Statement ID:** SIH26007
* **Category:** Hardware
* **Theme:** Smart Automation

---

## 📚 References

1. Official Smart India Hackathon Problem Statement — SIH26007
2. ESP32 technical documentation — Espressif
3. HC-SR04 ultrasonic sensor datasheet
4. NEO-6M GPS module documentation
5. L298N motor driver module documentation
6. Research on V2V/V2I communication and collision avoidance in mining environments

---

## 📜 Disclaimer

NETRA is an hackathon prototype demonstrating a mine-vehicle risk-awareness concept. It should not be used as the sole basis for operational, safety-critical, or autonomous decisions in an actual mining environment.

---

## ⭐ Project Concept in One Sentence

> **NETRA combines ultrasonic obstacle sensing, GPS positioning, geofenced risk zones, and deterministic risk fusion to provide real-time, multilingual safety alerts for a physically-driving mine-vehicle prototype operating alongside a simulated second vehicle.**
