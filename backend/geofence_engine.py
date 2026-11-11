"""
NETRA geofence engine.
Loads data/risk_zones.json and determines whether a given lat/lon falls
inside any defined risk zone (simple circular geofence: center + radius_m).
"""

import json
import math
from backend import config

_zones_cache = None


def _haversine_m(lat1, lon1, lat2, lon2):
    R = 6371000.0
    phi1, phi2 = math.radians(lat1), math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlambda = math.radians(lon2 - lon1)
    a = math.sin(dphi / 2) ** 2 + math.cos(phi1) * math.cos(phi2) * math.sin(dlambda / 2) ** 2
    return 2 * R * math.asin(math.sqrt(a))


def load_zones(force_reload=False):
    global _zones_cache
    if _zones_cache is None or force_reload:
        try:
            with open(config.RISK_ZONES_FILE, "r") as f:
                _zones_cache = json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            _zones_cache = []
    return _zones_cache


def check_zones(lat, lon):
    """
    Returns a list of zones the point currently falls inside (usually 0 or 1,
    but a list supports overlapping zones without losing information).
    """
    if lat is None or lon is None:
        return []
    hits = []
    for zone in load_zones():
        dist = _haversine_m(lat, lon, zone["latitude"], zone["longitude"])
        if dist <= zone["radius_m"]:
            hits.append(zone)
    return hits
