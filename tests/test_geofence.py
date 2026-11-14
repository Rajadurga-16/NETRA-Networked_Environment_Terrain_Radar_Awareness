from backend import geofence_engine


def test_point_inside_zone():
    geofence_engine.load_zones(force_reload=True)
    zones = geofence_engine.load_zones()
    assert len(zones) >= 1
    z = zones[0]
    hits = geofence_engine.check_zones(z["latitude"], z["longitude"])
    assert any(h["zone_id"] == z["zone_id"] for h in hits)


def test_point_far_outside_any_zone():
    hits = geofence_engine.check_zones(0.0, 0.0)  # nowhere near demo zones
    assert hits == []


def test_none_coordinates_return_no_hits():
    hits = geofence_engine.check_zones(None, None)
    assert hits == []
