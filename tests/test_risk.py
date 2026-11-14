from backend import risk_engine


def test_safe_when_far_and_not_approaching():
    collision = {"distance_m": 200, "state": "STATIONARY", "ttc_s": None}
    result = risk_engine.evaluate(collision, zone_hits=[], radar_target=False, radar_distance_m=None)
    assert result["risk_level"] == "SAFE"


def test_caution_band():
    collision = {"distance_m": 70, "state": "STATIONARY", "ttc_s": None}
    result = risk_engine.evaluate(collision, zone_hits=[], radar_target=False, radar_distance_m=None)
    assert result["risk_level"] in ("SAFE", "CAUTION")


def test_warning_when_approaching_and_mid_distance():
    collision = {"distance_m": 35, "state": "APPROACHING", "ttc_s": 15}
    result = risk_engine.evaluate(collision, zone_hits=[], radar_target=False, radar_distance_m=None)
    assert result["risk_level"] in ("WARNING", "CRITICAL")
    assert result["recommended_action"] in ("REDUCE SPEED", "STOP")


def test_critical_when_very_close_and_fast_approach():
    collision = {"distance_m": 10, "state": "APPROACHING", "ttc_s": 3}
    result = risk_engine.evaluate(collision, zone_hits=[], radar_target=False, radar_distance_m=None)
    assert result["risk_level"] == "CRITICAL"
    assert result["recommended_action"] == "STOP"


def test_zone_alert_raises_risk():
    collision = {"distance_m": 200, "state": "STATIONARY", "ttc_s": None}
    zone = [{"name": "Blind Curve", "severity": "HIGH"}]
    result = risk_engine.evaluate(collision, zone_hits=zone, radar_target=False, radar_distance_m=None)
    assert result["zone_alert"] is True
    assert result["risk_level"] != "SAFE"


def test_reasons_are_never_empty():
    collision = {"distance_m": None, "state": "UNKNOWN", "ttc_s": None}
    result = risk_engine.evaluate(collision, zone_hits=[], radar_target=False, radar_distance_m=None)
    assert len(result["reasons"]) >= 1
