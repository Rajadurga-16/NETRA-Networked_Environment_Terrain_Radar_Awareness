"""
NETRA collision/conflict engine.
Computes distance between Vehicle A and Vehicle B, tracks whether they are
approaching or receding, and estimates a simple time-to-conflict (ETA)
indicator. Deterministic, explainable - no black-box model.
"""

import math
import time


def haversine_m(lat1, lon1, lat2, lon2):
    R = 6371000.0
    phi1, phi2 = math.radians(lat1), math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlambda = math.radians(lon2 - lon1)
    a = math.sin(dphi / 2) ** 2 + math.cos(phi1) * math.cos(phi2) * math.sin(dlambda / 2) ** 2
    return 2 * R * math.asin(math.sqrt(a))


class CollisionEngine:
    """
    Keeps a short history of the A-B distance so it can compute a closing
    speed (m/s) from the actual rate of change, instead of guessing from
    each vehicle's individual speed/heading (which is less reliable with a
    cheap prototype GPS).
    """

    def __init__(self):
        self._last_distance = None
        self._last_time = None

    def compute(self, lat_a, lon_a, lat_b, lon_b):
        result = {
            "distance_m": None,
            "closing_speed_ms": 0.0,
            "state": "UNKNOWN",   # APPROACHING | RECEDING | STATIONARY | UNKNOWN
            "ttc_s": None,        # time-to-conflict, only set if approaching
        }

        if None in (lat_a, lon_a, lat_b, lon_b):
            return result

        distance = haversine_m(lat_a, lon_a, lat_b, lon_b)
        now = time.time()
        result["distance_m"] = round(distance, 1)

        if self._last_distance is not None and self._last_time is not None:
            dt = now - self._last_time
            if dt > 0.05:  # ignore ticks that are too close together (noise)
                delta = self._last_distance - distance  # positive = getting closer
                closing_speed = delta / dt
                result["closing_speed_ms"] = round(closing_speed, 2)

                if closing_speed > 0.3:
                    result["state"] = "APPROACHING"
                    result["ttc_s"] = round(distance / closing_speed, 1) if closing_speed > 0 else None
                elif closing_speed < -0.3:
                    result["state"] = "RECEDING"
                else:
                    result["state"] = "STATIONARY"

                self._last_distance = distance
                self._last_time = now
        else:
            self._last_distance = distance
            self._last_time = now

        return result

    def reset(self):
        self._last_distance = None
        self._last_time = None
