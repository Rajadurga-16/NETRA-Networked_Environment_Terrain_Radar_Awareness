"""
Run from the NETRA/ project root:  python -m pytest tests/
"""
import time
from backend.collision_engine import CollisionEngine, haversine_m


def test_haversine_zero_distance():
    assert haversine_m(18.0, 82.0, 18.0, 82.0) == 0


def test_haversine_known_small_distance():
    # ~0.001 deg latitude apart is roughly 111m
    d = haversine_m(18.0000, 82.0, 18.0010, 82.0)
    assert 100 < d < 125


def test_compute_first_call_has_no_state_yet():
    engine = CollisionEngine()
    result = engine.compute(18.0000, 82.0000, 18.0010, 82.0000)
    assert result["distance_m"] > 0
    # first call can't know closing speed yet (needs two samples)
    assert result["state"] in ("UNKNOWN",)


def test_compute_detects_approaching():
    engine = CollisionEngine()
    engine.compute(18.0000, 82.0000, 18.0050, 82.0000)  # far apart
    time.sleep(0.15)
    result = engine.compute(18.0000, 82.0000, 18.0010, 82.0000)  # much closer now
    assert result["state"] == "APPROACHING"
    assert result["closing_speed_ms"] > 0


def test_compute_missing_coords_returns_unknown():
    engine = CollisionEngine()
    result = engine.compute(None, None, 18.0, 82.0)
    assert result["distance_m"] is None
    assert result["state"] == "UNKNOWN"
