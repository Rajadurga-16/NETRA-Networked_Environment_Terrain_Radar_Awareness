"""
NETRA backend - main Flask application.
Run with:  python run.py   (from the NETRA/ project root)
"""

import time
import threading
from flask import Flask, jsonify, request, render_template

from backend import config, database, event_logger
from backend.serial_gateway import SerialGateway
from backend.vehicle_simulator import VehicleBSimulator
from backend.collision_engine import CollisionEngine
from backend import risk_engine, geofence_engine

app = Flask(
    __name__,
    template_folder="../dashboard/templates",
    static_folder="../dashboard/static",
)

gateway = SerialGateway()
vehicle_b = VehicleBSimulator()
collision_engine = CollisionEngine()

_state_lock = threading.Lock()
_latest_risk = {
    "risk_level": "SAFE",
    "risk_score": 0,
    "reasons": ["System starting up"],
    "recommended_action": "PROCEED",
    "zone_alert": False,
}
_active_zones = []
_last_a_risk_sent = None
_drive_requested = False  # set via /api/vehicle_a/drive - starts False (safety default)


def _background_loop():
    """
    Runs continuously:
      1. reads Vehicle A's latest telemetry from the serial gateway
      2. ticks Vehicle B's simulator
      3. computes collision + geofence + risk
      4. logs meaningful events
      5. sends risk feedback back to the ESP32 (buzzer/OLED/LED reaction)
    """
    global _latest_risk, _active_zones, _last_a_risk_sent, _drive_requested

    while True:
        a_state = gateway.get_latest()
        a_online = gateway.is_online()

        lat_a = a_state.get("latitude") if a_online else None
        lon_a = a_state.get("longitude") if a_online else None

        vehicle_b.tick(target_lat=lat_a, target_lon=lon_a)
        b_state = vehicle_b.get_state()

        if a_online and lat_a and lon_a and b_state["online"]:
            collision_result = collision_engine.compute(lat_a, lon_a, b_state["latitude"], b_state["longitude"])
        else:
            collision_result = {"distance_m": None, "closing_speed_ms": 0.0, "state": "UNKNOWN", "ttc_s": None}
            collision_engine.reset()

        zone_hits = geofence_engine.check_zones(lat_a, lon_a) if a_online else []

        radar_target = bool(a_state.get("radar_target")) if a_online else False
        radar_distance = a_state.get("radar_distance") if a_online else None
        if radar_distance is not None and radar_distance < 0:
            radar_distance = None

        if not a_online:
            risk_result = {
                "risk_level": "UNKNOWN",
                "risk_score": 0,
                "reasons": ["Vehicle A offline - DEGRADED MODE"],
                "recommended_action": "CHECK VEHICLE A LINK",
                "zone_alert": False,
            }
        else:
            risk_result = risk_engine.evaluate(collision_result, zone_hits, radar_target, radar_distance)

        with _state_lock:
            _latest_risk = risk_result
            _active_zones = zone_hits

        # SAFETY OVERRIDE: the dashboard's drive toggle is only ever a
        # permission, never a guarantee. Even if the operator has requested
        # driving, refuse to forward drive_enabled=true when risk is
        # CRITICAL/UNKNOWN or Vehicle A is offline - the ESP32 also has its
        # own local ultrasonic cutoff, but this is a second, independent layer.
        effective_drive_enabled = (
            _drive_requested
            and a_online
            and risk_result["risk_level"] not in ("CRITICAL", "UNKNOWN")
        )

        # Send feedback to ESP32 whenever anything relevant changes (avoid flooding serial)
        feedback_key = (risk_result["risk_level"], risk_result.get("zone_alert", False), effective_drive_enabled)
        if feedback_key != _last_a_risk_sent and a_online:
            msg = f"B:{collision_result.get('distance_m')}m {collision_result.get('state')}" \
                if collision_result.get("distance_m") is not None else risk_result["recommended_action"]
            gateway.send_feedback(risk_result["risk_level"], risk_result.get("zone_alert", False), msg,
                                   drive_enabled=effective_drive_enabled)
            _last_a_risk_sent = feedback_key

            if risk_result["risk_level"] in ("WARNING", "CRITICAL"):
                event_logger.log_event(
                    vehicle_id="A",
                    event_type="RISK_CHANGE",
                    latitude=lat_a, longitude=lon_a,
                    risk_level=risk_result["risk_level"],
                    distance=collision_result.get("distance_m"),
                    reason="; ".join(risk_result["reasons"]),
                )
            if risk_result.get("zone_alert"):
                event_logger.log_event(
                    vehicle_id="A",
                    event_type="ZONE_ALERT",
                    latitude=lat_a, longitude=lon_a,
                    risk_level=risk_result["risk_level"],
                    distance=None,
                    reason="; ".join(z.get("name", "") for z in zone_hits),
                )

        time.sleep(config.SIM_TICK_S)


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/state")
def api_state():
    a_state = gateway.get_latest()
    a_online = gateway.is_online()
    with _state_lock:
        risk = dict(_latest_risk)
        zones = list(_active_zones)

    return jsonify({
        "system_mode": "NORMAL" if a_online else "DEGRADED",
        "vehicle_a": {
            "kind": "REAL",
            "online": a_online,
            "latitude": a_state.get("latitude"),
            "longitude": a_state.get("longitude"),
            "speed": a_state.get("speed"),
            "heading": a_state.get("heading"),
            "gps_status": a_state.get("gps_status", "UNKNOWN"),
            "radar_status": a_state.get("radar_status", "UNKNOWN"),
            "radar_target": a_state.get("radar_target", False),
            "radar_distance": a_state.get("radar_distance"),
            "driving": a_state.get("driving", False),
            "drive_requested": _drive_requested,
        },
        "vehicle_b": vehicle_b.get_state(),
        "risk": risk,
        "active_zones": [{"name": z.get("name"), "hazard_type": z.get("hazard_type"),
                           "severity": z.get("severity"), "recommended_speed": z.get("recommended_speed")}
                          for z in zones],
        "all_zones": geofence_engine.load_zones(),
        "recent_events": event_logger.get_recent(10),
    })


@app.route("/api/vehicle_b/manual", methods=["POST"])
def api_vehicle_b_manual():
    data = request.get_json(force=True, silent=True) or {}
    vehicle_b.set_manual(
        lat=data.get("latitude"),
        lon=data.get("longitude"),
        speed_kmph=data.get("speed"),
        heading_deg=data.get("heading"),
    )
    return jsonify({"ok": True, "state": vehicle_b.get_state()})


@app.route("/api/vehicle_b/mode", methods=["POST"])
def api_vehicle_b_mode():
    data = request.get_json(force=True, silent=True) or {}
    mode = data.get("mode", "MANUAL")
    vehicle_b.set_mode(mode)
    return jsonify({"ok": True, "state": vehicle_b.get_state()})


@app.route("/api/vehicle_b/offline", methods=["POST"])
def api_vehicle_b_offline():
    data = request.get_json(force=True, silent=True) or {}
    vehicle_b.set_offline(bool(data.get("offline", False)))
    return jsonify({"ok": True, "state": vehicle_b.get_state()})


@app.route("/api/vehicle_a/drive", methods=["POST"])
def api_vehicle_a_drive():
    """
    Operator permission to drive. This is a REQUEST only - the background
    loop's safety override (and the ESP32's own local ultrasonic cutoff)
    can still refuse to actually enable motors. See _background_loop().
    """
    global _drive_requested
    data = request.get_json(force=True, silent=True) or {}
    _drive_requested = bool(data.get("enabled", False))
    return jsonify({"ok": True, "drive_requested": _drive_requested})


@app.route("/api/events")
def api_events():
    return jsonify(event_logger.get_recent(config.EVENT_HISTORY_LIMIT))


def start_background_services():
    database.init_db()
    event_logger.load_from_db()
    gateway.start()
    t = threading.Thread(target=_background_loop, daemon=True)
    t.start()


if __name__ == "__main__":
    start_background_services()
    app.run(host=config.FLASK_HOST, port=config.FLASK_PORT, debug=False, use_reloader=False)
