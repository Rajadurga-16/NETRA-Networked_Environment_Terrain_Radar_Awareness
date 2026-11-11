"""
NETRA risk engine.
Deterministic, explainable rules only - no black-box model, per project spec.
Combines the collision-engine result (distance/closing speed/ttc) with the
geofence result (is Vehicle A inside a risk zone) into one risk verdict.
"""

from backend import config


def evaluate(collision_result: dict, zone_hits: list, radar_target: bool, radar_distance_m):
    """
    collision_result: output of CollisionEngine.compute()
    zone_hits: output of geofence_engine.check_zones()
    radar_target: bool, whether Vehicle A's onboard radar currently sees an object
    radar_distance_m: float or None, radar-reported distance to that object

    Returns dict: risk_level, risk_score (0-100), reasons (list[str]), recommended_action
    """
    reasons = []
    score = 0
    distance = collision_result.get("distance_m")
    state = collision_result.get("state")
    ttc = collision_result.get("ttc_s")

    # ---- Vehicle-to-vehicle contribution ----
    if distance is not None:
        if distance <= config.DIST_CRITICAL_M:
            score += 60
            reasons.append(f"Vehicle B distance {distance}m (critical band)")
        elif distance <= config.DIST_WARNING_M:
            score += 40
            reasons.append(f"Vehicle B distance {distance}m (warning band)")
        elif distance <= config.DIST_CAUTION_M:
            score += 20
            reasons.append(f"Vehicle B distance {distance}m (caution band)")

        if state == "APPROACHING":
            reasons.append("Vehicle B approaching")
            score += 15
            if ttc is not None:
                if ttc <= config.TTC_CRITICAL_S:
                    score += 25
                    reasons.append(f"Time-to-conflict {ttc}s (critical)")
                elif ttc <= config.TTC_WARNING_S:
                    score += 10
                    reasons.append(f"Time-to-conflict {ttc}s (warning)")

    # ---- Local ultrasonic obstacle contribution (independent close-range check) ----
    # NOTE: "radar_*" naming kept for compatibility with the serial protocol/
    # dashboard, but this is the HC-SR04 ultrasonic sensor on Vehicle A -
    # thresholds are tabletop-robot scale (centimeters), not vehicle scale.
    if radar_target and radar_distance_m is not None:
        if radar_distance_m <= config.ULTRASONIC_VERY_CLOSE_M:
            score += 30
            reasons.append(f"Obstacle at {radar_distance_m}m (very close)")
        elif radar_distance_m <= config.ULTRASONIC_NEARBY_M:
            score += 15
            reasons.append(f"Obstacle at {radar_distance_m}m (nearby)")

    # ---- Geofence contribution ----
    zone_alert = False
    if zone_hits:
        zone_alert = True
        for zone in zone_hits:
            sev = zone.get("severity", "MEDIUM")
            reasons.append(f"Inside risk zone: {zone.get('name')} ({sev})")
            if sev == "HIGH":
                score += 35
            elif sev == "MEDIUM":
                score += 20
            else:
                score += 10

    score = min(score, 100)

    if score >= 70:
        level = "CRITICAL"
        action = "STOP"
    elif score >= 45:
        level = "WARNING"
        action = "REDUCE SPEED"
    elif score >= 20:
        level = "CAUTION"
        action = "PROCEED WITH CAUTION"
    else:
        level = "SAFE"
        action = "PROCEED"
        if not reasons:
            reasons.append("No nearby vehicles or hazards detected")

    return {
        "risk_level": level,
        "risk_score": score,
        "reasons": reasons,
        "recommended_action": action,
        "zone_alert": zone_alert,
    }
