"""
NETRA serial gateway.
Background thread that:
  - opens the USB serial port to Vehicle A's ESP32
  - reads one JSON telemetry line at a time, safely (never crashes on
    malformed/partial lines)
  - keeps the latest known Vehicle A state available to the rest of the app
  - sends risk-feedback JSON lines back to the ESP32 (risk_level, zone_alert)

If the port can't be opened, or the link drops, this module reports
Vehicle A as OFFLINE rather than crashing the whole backend - the dashboard
and risk engine must handle that gracefully (DEGRADED MODE).
"""

import json
import threading
import time

try:
    import serial
except ImportError:
    serial = None  # pyserial not installed yet - handled at runtime, see app.py startup check

from backend import config


class SerialGateway:
    def __init__(self, port=None, baud=None):
        self.port = port or config.SERIAL_PORT
        self.baud = baud or config.SERIAL_BAUD
        self._ser = None
        self._lock = threading.Lock()
        self._latest = {}
        self._last_rx_time = 0
        self._running = False
        self._thread = None

    def start(self):
        self._running = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False
        if self._ser:
            try:
                self._ser.close()
            except Exception:
                pass

    def is_online(self):
        return (time.time() - self._last_rx_time) < config.VEHICLE_A_TIMEOUT_S

    def get_latest(self):
        with self._lock:
            return dict(self._latest)

    def send_feedback(self, risk_level: str, zone_alert: bool, message: str = "", drive_enabled: bool = False):
        if self._ser is None:
            return False
        payload = json.dumps({
            "risk_level": risk_level,
            "zone_alert": zone_alert,
            "message": message[:40],
            "drive_enabled": bool(drive_enabled),
        }) + "\n"
        try:
            self._ser.write(payload.encode("utf-8"))
            return True
        except Exception:
            return False

    def _run(self):
        while self._running:
            if self._ser is None:
                if serial is None:
                    time.sleep(2)
                    continue
                try:
                    self._ser = serial.Serial(self.port, self.baud, timeout=config.SERIAL_TIMEOUT_S)
                    print(f"[serial_gateway] Connected to {self.port} @ {self.baud}")
                except Exception as e:
                    print(f"[serial_gateway] Could not open {self.port}: {e}. Retrying in 3s. "
                          f"(Vehicle A will show OFFLINE / DEGRADED MODE until connected.)")
                    self._ser = None
                    time.sleep(3)
                    continue

            try:
                line = self._ser.readline().decode("utf-8", errors="ignore").strip()
                if not line:
                    continue
                try:
                    data = json.loads(line)
                except json.JSONDecodeError:
                    continue  # malformed line - drop it, never crash

                with self._lock:
                    self._latest = data
                    self._last_rx_time = time.time()

            except Exception as e:
                print(f"[serial_gateway] Serial read error: {e}. Reconnecting.")
                try:
                    self._ser.close()
                except Exception:
                    pass
                self._ser = None
                time.sleep(2)
