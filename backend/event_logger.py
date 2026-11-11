"""
NETRA event logger.
Wraps database.py: every call both persists to SQLite and keeps a small
in-memory ring buffer so the dashboard's /api/state endpoint doesn't have
to hit the DB every second.
"""

from datetime import datetime, timezone
from collections import deque
from backend import database, config

_recent = deque(maxlen=config.EVENT_HISTORY_LIMIT)


def log_event(vehicle_id: str, event_type: str, latitude=None, longitude=None,
              risk_level=None, distance=None, reason=""):
    event = {
        "timestamp": datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S"),
        "vehicle_id": vehicle_id,
        "event_type": event_type,
        "latitude": latitude,
        "longitude": longitude,
        "risk_level": risk_level,
        "distance": distance,
        "reason": reason,
    }
    database.insert_event(event)
    _recent.appendleft(event)
    return event


def get_recent(limit: int = None):
    if limit is None:
        return list(_recent)
    return list(_recent)[:limit]


def load_from_db():
    """Call once at startup to warm the in-memory buffer from SQLite."""
    rows = database.get_recent_events(config.EVENT_HISTORY_LIMIT)
    _recent.clear()
    for row in rows:
        _recent.append(row)
