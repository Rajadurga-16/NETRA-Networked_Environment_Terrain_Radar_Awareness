"""
NETRA Vehicle B simulator.
Vehicle B has NO physical hardware - it exists only here, on the laptop.
Two modes:
  MANUAL    - dashboard/API sets lat/lon/speed/heading directly.
  AUTO      - Vehicle B gradually closes distance toward Vehicle A's last
              known position, so you can demo SAFE -> CAUTION -> WARNING ->
              CRITICAL without touching anything by hand.
"""

import math
import time
import threading
from backend import config

METERS_PER_DEG_LAT = 111320.0


def _meters_per_deg_lon(lat):
    return 111320.0 * math.cos(math.radians(lat))


class VehicleBSimulator:
    def __init__(self, start_lat=18.00100, start_lon=82.00100):
        self.lock = threading.Lock()
        self.mode = "MANUAL"  # "MANUAL" | "AUTO"
        self.lat = start_lat
        self.lon = start_lon
        self.speed_kmph = 0.0
        self.heading_deg = 0.0
        self.online = True
        self._last_tick = time.time()

    def set_manual(self, lat=None, lon=None, speed_kmph=None, heading_deg=None):
        with self.lock:
            self.mode = "MANUAL"
            if lat is not None:
                self.lat = lat
            if lon is not None:
                self.lon = lon
            if speed_kmph is not None:
                self.speed_kmph = speed_kmph
            if heading_deg is not None:
                self.heading_deg = heading_deg

    def set_mode(self, mode: str):
        with self.lock:
            if mode in ("MANUAL", "AUTO"):
                self.mode = mode

    def set_offline(self, offline: bool):
        with self.lock:
            self.online = not offline

    def get_state(self):
        with self.lock:
            return {
                "vehicle_id": "B",
                "kind": "SIMULATED",
                "latitude": self.lat,
                "longitude": self.lon,
                "speed": self.speed_kmph,
                "heading": self.heading_deg,
                "mode": self.mode,
                "online": self.online,
            }

    def tick(self, target_lat=None, target_lon=None):
        """
        Call this roughly once per SIM_TICK_S seconds (driven by a background
        thread in app.py). In AUTO mode, moves B toward (target_lat, target_lon)
        - normally Vehicle A's current position - at SIM_AUTO_APPROACH_SPEED_MS.
        In MANUAL mode this is a no-op (position only changes via set_manual).
        """
        with self.lock:
            if self.mode != "AUTO" or target_lat is None or target_lon is None or not self.online:
                return

            now = time.time()
            dt = now - self._last_tick
            self._last_tick = now
            if dt <= 0:
                return

            dlat_m = (target_lat - self.lat) * METERS_PER_DEG_LAT
            dlon_m = (target_lon - self.lon) * _meters_per_deg_lon(self.lat)
            dist = math.sqrt(dlat_m ** 2 + dlon_m ** 2)

            step_m = config.SIM_AUTO_APPROACH_SPEED_MS * dt
            if dist < 1.0:
                # Reached target - hold position (don't overshoot/oscillate)
                self.speed_kmph = 0.0
                return

            frac = min(step_m / dist, 1.0)
            self.lat += (target_lat - self.lat) * frac
            self.lon += (target_lon - self.lon) * frac

            self.heading_deg = (math.degrees(math.atan2(dlon_m, dlat_m)) + 360) % 360
            self.speed_kmph = config.SIM_AUTO_APPROACH_SPEED_MS * 3.6
