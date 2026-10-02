const POLL_MS = 1000;
let bOffline = false;

function setText(id, value) {
  const el = document.getElementById(id);
  if (el) el.textContent = value;
}

function riskClass(level) {
  switch (level) {
    case "SAFE": return "risk-safe";
    case "CAUTION": return "risk-caution";
    case "WARNING": return "risk-warning";
    case "CRITICAL": return "risk-critical";
    default: return "risk-unknown";
  }
}

async function pollState() {
  try {
    const res = await fetch("/api/state");
    const state = await res.json();
    render(state);
  } catch (e) {
    console.error("poll failed", e);
  } finally {
    setTimeout(pollState, POLL_MS);
  }
}

function render(state) {
  // system mode
  const modeEl = document.getElementById("systemMode");
  modeEl.textContent = state.system_mode;
  modeEl.className = "mode-badge " + (state.system_mode === "NORMAL" ? "mode-normal" : "mode-degraded");

  // vehicle A
  const a = state.vehicle_a || {};
  setText("aGps", a.gps_status || "-");
  setText("aRadar", a.radar_status || "-");
  setText("aRadarTarget", a.radar_target ? `YES (${a.radar_distance}m)` : "no target");
  setText("aDriving", a.driving ? "MOVING" : (a.drive_requested ? "permitted, not moving" : "stopped"));

  const driveBtn = document.getElementById("driveToggle");
  if (driveBtn && !driveBtnBusy) {
    if (a.drive_requested) {
      driveBtn.textContent = "■ Stop Driving";
      driveBtn.className = "drive-btn drive-driving";
    } else {
      driveBtn.textContent = "▶ Start Driving";
      driveBtn.className = "drive-btn drive-stopped";
    }
  }
  setText("aSpeed", a.speed != null ? `${a.speed.toFixed(1)} km/h` : "-");
  setText("aHeading", a.heading != null ? `${a.heading.toFixed(0)}°` : "-");
  setText("aLink", a.online ? "CONNECTED" : "OFFLINE");

  // vehicle B
  const b = state.vehicle_b || {};
  setText("bMode", b.mode || "-");
  setText("bSpeed", b.speed != null ? `${Number(b.speed).toFixed(1)} km/h` : "-");
  setText("bHeading", b.heading != null ? `${Number(b.heading).toFixed(0)}°` : "-");
  setText("bOnline", b.online ? "ONLINE" : "OFFLINE (simulated)");

  // collision (distance/severity/direction between A and B - straight from
  // the collision engine, not parsed out of risk "reasons" text)
  const collision = state.collision || {};
  setText("bDistance", collision.distance_m != null ? `${collision.distance_m} m` : "-");
  setText("bSeverity", collision.proximity_severity || "-");
  setText("bMovement", collision.state || "-");

  // risk
  const risk = state.risk || {};
  const riskEl = document.getElementById("riskLevel");
  riskEl.textContent = risk.risk_level || "UNKNOWN";
  riskEl.className = "risk-badge " + riskClass(risk.risk_level);

  setText("riskDistance", collision.distance_m != null ? `${collision.distance_m} m` : "-");
  setText("riskMovement", collision.state || "-");
  setText("riskScore", risk.risk_score != null ? risk.risk_score : "-");
  setText("riskAction", risk.recommended_action || "-");
  setText("riskSpeed", risk.recommended_speed_kmph != null ? `${risk.recommended_speed_kmph} km/h` : "-");
  setText("riskSteering", risk.steering_advice || "No action needed");

  const reasonsEl = document.getElementById("riskReasons");
  reasonsEl.innerHTML = "";
  (risk.reasons || []).forEach(r => {
    const div = document.createElement("div");
    div.textContent = "• " + r;
    reasonsEl.appendChild(div);
  });

  const zoneAlertEl = document.getElementById("zoneAlert");
  if (risk.zone_alert) {
    zoneAlertEl.classList.remove("hidden");
    zoneAlertEl.textContent = "⚠ " + (state.active_zones || []).map(z => z.name).join(", ");
  } else {
    zoneAlertEl.classList.add("hidden");
  }

  // environment (fog - manual/demo input)
  const env = state.environment || {};
  const fogBadge = document.getElementById("fogBadge");
  if (fogBadge) {
    fogBadge.textContent = "FOG: " + (env.fog_severity || "NONE");
    fogBadge.className = "fog-badge fog-" + (env.fog_severity || "NONE");
  }
  const fogSelect = document.getElementById("fogSelect");
  if (fogSelect && !fogSelectBusy && env.fog_severity && fogSelect.value !== env.fog_severity) {
    fogSelect.value = env.fog_severity;
  }

  // events
  const tbody = document.getElementById("eventsBody");
  tbody.innerHTML = "";
  (state.recent_events || []).forEach(ev => {
    const tr = document.createElement("tr");
    tr.innerHTML = `<td>${ev.timestamp}</td><td>${ev.vehicle_id || ""}</td><td>${ev.event_type || ""}</td>` +
                    `<td>${ev.risk_level || ""}</td><td>${ev.distance != null ? ev.distance + "m" : ""}</td>` +
                    `<td>${ev.reason || ""}</td>`;
    tbody.appendChild(tr);
  });

  drawMap(state);
  updateVoiceGuidance(state);
}

document.getElementById("voiceLangSelect").addEventListener("change", (e) => {
  setVoiceLanguage(e.target.value);
});

document.getElementById("voiceEnabledCheck").addEventListener("change", (e) => {
  setVoiceEnabled(e.target.checked);
});

document.getElementById("testVoiceBtn").addEventListener("click", () => {
  const wasEnabled = document.getElementById("voiceEnabledCheck").checked;
  if (!wasEnabled) return;
  speak("WARNING");
});

document.getElementById("modeSelect").addEventListener("change", async (e) => {
  await fetch("/api/vehicle_b/mode", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ mode: e.target.value }),
  });
});

document.getElementById("applyManual").addEventListener("click", async () => {
  const lat = parseFloat(document.getElementById("manLat").value);
  const lon = parseFloat(document.getElementById("manLon").value);
  const speed = parseFloat(document.getElementById("manSpeed").value);
  document.getElementById("modeSelect").value = "MANUAL";
  await fetch("/api/vehicle_b/manual", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ latitude: lat, longitude: lon, speed: speed }),
  });
});

document.getElementById("toggleOffline").addEventListener("click", async (e) => {
  bOffline = !bOffline;
  e.target.textContent = bOffline ? "Bring B Online" : "Simulate B Offline";
  await fetch("/api/vehicle_b/offline", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ offline: bOffline }),
  });
});

let driveBtnBusy = false;
let driveRequestedLocal = false;

document.getElementById("driveToggle").addEventListener("click", async (e) => {
  driveBtnBusy = true;
  driveRequestedLocal = !driveRequestedLocal;
  e.target.textContent = driveRequestedLocal ? "■ Stop Driving" : "▶ Start Driving";
  e.target.className = "drive-btn " + (driveRequestedLocal ? "drive-driving" : "drive-stopped");
  await fetch("/api/vehicle_a/drive", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ enabled: driveRequestedLocal }),
  });
  driveBtnBusy = false;
});

let fogSelectBusy = false;

document.getElementById("fogSelect").addEventListener("change", async (e) => {
  fogSelectBusy = true;
  await fetch("/api/environment/fog", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ severity: e.target.value }),
  });
  fogSelectBusy = false;
});

pollState();
