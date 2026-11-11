"""
NETRA database module - SQLite storage for event history.
Minimal MVP wrapper, no ORM, so it's easy to read/debug during the hackathon.
"""

import sqlite3
import os
import threading
from backend import config

_lock = threading.Lock()


def _connect():
    os.makedirs(os.path.dirname(config.DB_PATH), exist_ok=True)
    conn = sqlite3.connect(config.DB_PATH, check_same_thread=False)
    return conn


def init_db():
    with _lock:
        conn = _connect()
        conn.execute("""
            CREATE TABLE IF NOT EXISTS events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                vehicle_id TEXT,
                event_type TEXT,
                latitude REAL,
                longitude REAL,
                risk_level TEXT,
                distance REAL,
                reason TEXT
            )
        """)
        conn.commit()
        conn.close()


def insert_event(event: dict):
    with _lock:
        conn = _connect()
        conn.execute(
            """INSERT INTO events
               (timestamp, vehicle_id, event_type, latitude, longitude, risk_level, distance, reason)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
            (
                event.get("timestamp"),
                event.get("vehicle_id"),
                event.get("event_type"),
                event.get("latitude"),
                event.get("longitude"),
                event.get("risk_level"),
                event.get("distance"),
                event.get("reason"),
            ),
        )
        conn.commit()
        conn.close()


def get_recent_events(limit: int = 50):
    with _lock:
        conn = _connect()
        cur = conn.execute(
            "SELECT timestamp, vehicle_id, event_type, latitude, longitude, risk_level, distance, reason "
            "FROM events ORDER BY id DESC LIMIT ?",
            (limit,),
        )
        rows = cur.fetchall()
        conn.close()
    cols = ["timestamp", "vehicle_id", "event_type", "latitude", "longitude", "risk_level", "distance", "reason"]
    return [dict(zip(cols, row)) for row in rows]
