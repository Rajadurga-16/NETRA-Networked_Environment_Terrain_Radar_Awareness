// NETRA local digital mine map.
// Pure canvas, no external tiles/internet dependency (offline-safe by design).
// Plots lat/lon as flat-earth meter offsets from a fixed reference point -
// fine for a small demo area, not for large-scale real mapping.

const REF_LAT = 18.00010;
const REF_LON = 82.00010;
const METERS_PER_DEG_LAT = 111320.0;

function metersPerDegLon(lat) {
  return 111320.0 * Math.cos(lat * Math.PI / 180);
}

function toXY(lat, lon, canvas, metersPerPixel) {
  const dLatM = (lat - REF_LAT) * METERS_PER_DEG_LAT;
  const dLonM = (lon - REF_LON) * metersPerDegLon(REF_LAT);
  const cx = canvas.width / 2;
  const cy = canvas.height / 2;
  return {
    x: cx + dLonM / metersPerPixel,
    y: cy - dLatM / metersPerPixel, // screen Y is inverted vs. latitude
  };
}

const trailA = [];
const trailB = [];
const MAX_TRAIL = 40;

// Risk-memory heatmap: past WARNING/CRITICAL/zone-alert locations, drawn as
// soft translucent blobs BEHIND everything else, so the driver can see
// "this spot has caused trouble before" even before Vehicle B is anywhere
// nearby. Built entirely from events already logged (backend /api/state's
// risk_history), not a live sensor - it's literally memory, not detection.
function drawRiskHeatmap(ctx, canvas, points, metersPerPixel) {
  if (!points || points.length === 0) return;
  ctx.save();
  points.forEach(pt => {
    const p = toXY(pt.latitude, pt.longitude, canvas, metersPerPixel);
    const radius = pt.risk_level === "CRITICAL" ? 26 : 18;
    const grad = ctx.createRadialGradient(p.x, p.y, 0, p.x, p.y, radius);
    const color = pt.risk_level === "CRITICAL" ? "200,64,47" : "217,119,43";
    grad.addColorStop(0, `rgba(${color},0.30)`);
    grad.addColorStop(1, `rgba(${color},0)`);
    ctx.fillStyle = grad;
    ctx.beginPath();
    ctx.arc(p.x, p.y, radius, 0, Math.PI * 2);
    ctx.fill();
  });
  ctx.restore();
}

function drawMap(state) {
  const canvas = document.getElementById("mineMap");
  if (!canvas) return;
  const ctx = canvas.getContext("2d");
  const metersPerPixel = 1.2; // zoom level - tune if your demo area is bigger/smaller

  ctx.clearRect(0, 0, canvas.width, canvas.height);

  // background grid
  ctx.strokeStyle = "#2a221a";
  ctx.lineWidth = 1;
  for (let x = 0; x < canvas.width; x += 30) {
    ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, canvas.height); ctx.stroke();
  }
  for (let y = 0; y < canvas.height; y += 30) {
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(canvas.width, y); ctx.stroke();
  }

  // risk-memory heatmap (drawn first, so it sits underneath everything else)
  drawRiskHeatmap(ctx, canvas, state.risk_history, metersPerPixel);

  // risk zones
  (state.all_zones || []).forEach(zone => {
    const p = toXY(zone.latitude, zone.longitude, canvas, metersPerPixel);
    const r = zone.radius_m / metersPerPixel;
    ctx.beginPath();
    ctx.arc(p.x, p.y, r, 0, Math.PI * 2);
    ctx.fillStyle = zone.severity === "HIGH" ? "rgba(217,119,43,0.15)" : "rgba(224,169,46,0.12)";
    ctx.fill();
    ctx.strokeStyle = zone.severity === "HIGH" ? "#d9772b" : "#e0a92e";
    ctx.stroke();
    ctx.fillStyle = "#a9998a";
    ctx.font = "10px sans-serif";
    ctx.fillText(zone.name, p.x - r, p.y - r - 4);
  });

  // vehicle A
  if (state.vehicle_a && state.vehicle_a.latitude && state.vehicle_a.online) {
    const p = toXY(state.vehicle_a.latitude, state.vehicle_a.longitude, canvas, metersPerPixel);
    trailA.push(p);
    if (trailA.length > MAX_TRAIL) trailA.shift();
    drawTrail(ctx, trailA, "#e8a13a");
    drawVehicleDot(ctx, p, "#e8a13a", "A");
  }

  // vehicle B
  if (state.vehicle_b && state.vehicle_b.latitude && state.vehicle_b.online) {
    const p = toXY(state.vehicle_b.latitude, state.vehicle_b.longitude, canvas, metersPerPixel);
    trailB.push(p);
    if (trailB.length > MAX_TRAIL) trailB.shift();
    drawTrail(ctx, trailB, "#7a9bb0");
    drawVehicleDot(ctx, p, "#7a9bb0", "B");
  }
}

function drawTrail(ctx, trail, color) {
  if (trail.length < 2) return;
  ctx.strokeStyle = color;
  ctx.globalAlpha = 0.35;
  ctx.beginPath();
  ctx.moveTo(trail[0].x, trail[0].y);
  for (let i = 1; i < trail.length; i++) ctx.lineTo(trail[i].x, trail[i].y);
  ctx.stroke();
  ctx.globalAlpha = 1.0;
}

function drawVehicleDot(ctx, p, color, label) {
  ctx.beginPath();
  ctx.arc(p.x, p.y, 7, 0, Math.PI * 2);
  ctx.fillStyle = color;
  ctx.fill();
  ctx.strokeStyle = "#fff";
  ctx.lineWidth = 1.5;
  ctx.stroke();
  ctx.fillStyle = "#fff";
  ctx.font = "bold 10px sans-serif";
  ctx.fillText(label, p.x - 3, p.y - 10);
}
