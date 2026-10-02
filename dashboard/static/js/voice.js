// NETRA voice guidance.
// Uses the browser's built-in Web Speech API (speechSynthesis) - runs on the
// laptop/dashboard side, so it needs NO extra hardware (no DFPlayer/speaker
// on Vehicle A - those are optional future/pilot features per the MVP spec).
//
// Language/voice availability depends on the OS + browser. Chrome on
// Windows/Android generally ships Hindi (hi-IN) and Tamil (ta-IN) voices;
// if a language isn't installed, the browser silently falls back to a
// default voice, and speech still works but with English pronunciation of
// the text - the caption/subtitle in the "Voice Alert" panel keeps working
// regardless of whether the audio itself does.

const VOICE_MESSAGES = {
  en: {
    lang: "en-IN",
    SAFE: "Path is clear.",
    CAUTION: "Caution. Another vehicle is nearby.",
    WARNING: "Warning. Vehicle approaching. Reduce speed.",
    CRITICAL: "Critical alert. Stop immediately.",
    ZONE: "Entering high risk zone. Blind curve ahead. Reduce speed.",
    RADAR_OBSTACLE: "Obstacle detected close to the vehicle.",
    OFFLINE: "Vehicle A is offline. System in degraded mode.",
    STEER_LEFT: "Keep left. Vehicle approaching from your right.",
    STEER_RIGHT: "Keep right. Vehicle approaching from your left.",
    STEER_AHEAD: "Vehicle ahead. Slow down.",
    FOG_HIGH: "Heavy fog conditions set. Reduce speed and increase following distance.",
  },
  hi: {
    lang: "hi-IN",
    SAFE: "मार्ग साफ है।",
    CAUTION: "सावधान। पास में एक वाहन है।",
    WARNING: "चेतावनी। वाहन पास आ रहा है। गति कम करें।",
    CRITICAL: "गंभीर चेतावनी। तुरंत रुकें।",
    ZONE: "उच्च जोखिम क्षेत्र में प्रवेश। आगे अंधा मोड़ है। गति कम करें।",
    RADAR_OBSTACLE: "वाहन के पास बाधा मिली है।",
    OFFLINE: "वाहन ए ऑफलाइन है। सिस्टम में गड़बड़ी है।",
    STEER_LEFT: "बाईं ओर रहें। वाहन दाईं ओर से आ रहा है।",
    STEER_RIGHT: "दाईं ओर रहें। वाहन बाईं ओर से आ रहा है।",
    STEER_AHEAD: "वाहन सामने है। धीमे चलें।",
    FOG_HIGH: "घना कोहरा सेट किया गया है। गति कम करें।",
  },
  ta: {
    lang: "ta-IN",
    SAFE: "பாதை தெளிவாக உள்ளது.",
    CAUTION: "எச்சரிக்கை. அருகில் ஒரு வாகனம் உள்ளது.",
    WARNING: "எச்சரிக்கை. வாகனம் நெருங்குகிறது. வேகத்தைக் குறைக்கவும்.",
    CRITICAL: "அவசர எச்சரிக்கை. உடனே நிறுத்தவும்.",
    ZONE: "உயர் ஆபத்து மண்டலத்தில் நுழைகிறீர்கள். முன்னால் குருட்டு வளைவு. வேகத்தைக் குறைக்கவும்.",
    RADAR_OBSTACLE: "வாகனத்திற்கு அருகில் தடை கண்டறியப்பட்டது.",
    OFFLINE: "வாகனம் ஏ ஆஃப்லைனில் உள்ளது. கணினி சீர்குலைந்துள்ளது.",
    STEER_LEFT: "இடதுபுறம் நகரவும். வாகனம் வலதுபுறத்தில் இருந்து நெருங்குகிறது.",
    STEER_RIGHT: "வலதுபுறம் நகரவும். வாகனம் இடதுபுறத்தில் இருந்து நெருங்குகிறது.",
    STEER_AHEAD: "வாகனம் முன்னால் உள்ளது. வேகத்தைக் குறைக்கவும்.",
    FOG_HIGH: "அடர் மூடுபனி நிலை அமைக்கப்பட்டுள்ளது. வேகத்தைக் குறைக்கவும்.",
  },
};

let voiceEnabled = true;
let voiceLang = "en";

// Cooldowns so the voice doesn't repeat every poll (1s) - only on real
// changes, plus a periodic repeat for CRITICAL since that one matters most.
let lastSpokenKey = null;
let lastCriticalRepeat = 0;
const CRITICAL_REPEAT_MS = 6000;
let lastRadarSpoken = 0;
const RADAR_REPEAT_MS = 8000;
let lastSteerSpoken = null;
let lastSteerRepeat = 0;
const STEER_REPEAT_MS = 7000;
let lastFogSpoken = null;

function setVoiceLanguage(lang) {
  if (VOICE_MESSAGES[lang]) voiceLang = lang;
}

function setVoiceEnabled(enabled) {
  voiceEnabled = enabled;
  if (!enabled && window.speechSynthesis) {
    window.speechSynthesis.cancel();
  }
}

function speak(messageKey) {
  if (!voiceEnabled || !window.speechSynthesis) return;
  const pack = VOICE_MESSAGES[voiceLang] || VOICE_MESSAGES.en;
  const text = pack[messageKey];
  if (!text) return;

  const utter = new SpeechSynthesisUtterance(text);
  utter.lang = pack.lang;
  utter.rate = 1.0;
  window.speechSynthesis.cancel(); // don't queue/overlap stale alerts
  window.speechSynthesis.speak(utter);

  const captionEl = document.getElementById("voiceCaption");
  if (captionEl) captionEl.textContent = text;
}

function steeringToVoiceKey(advice) {
  if (!advice) return null;
  if (advice.includes("KEEP LEFT")) return "STEER_LEFT";
  if (advice.includes("KEEP RIGHT")) return "STEER_RIGHT";
  if (advice.includes("AHEAD")) return "STEER_AHEAD";
  return null;
}

// Called from dashboard.js render() on every state poll.
function updateVoiceGuidance(state) {
  if (!voiceEnabled) return;
  const risk = state.risk || {};
  const level = risk.risk_level || "UNKNOWN";
  const zoneAlert = !!risk.zone_alert;
  const now = Date.now();

  // Vehicle A offline takes priority over everything else.
  if (state.system_mode === "DEGRADED") {
    if (lastSpokenKey !== "OFFLINE") { speak("OFFLINE"); lastSpokenKey = "OFFLINE"; }
    return;
  }

  const key = zoneAlert ? "ZONE" : level;

  if (key !== lastSpokenKey) {
    if (VOICE_MESSAGES.en[key]) {
      speak(key);
      lastSpokenKey = key;
      if (key === "CRITICAL") lastCriticalRepeat = now;
    }
  } else if (level === "CRITICAL" && now - lastCriticalRepeat > CRITICAL_REPEAT_MS) {
    speak("CRITICAL");
    lastCriticalRepeat = now;
  }

  // Steering guidance (which side to move to) - only when it changes or on
  // a slow repeat, so it doesn't talk over the main risk-level announcement.
  const steerKey = steeringToVoiceKey(risk.steering_advice);
  if (steerKey && (steerKey !== lastSteerSpoken || now - lastSteerRepeat > STEER_REPEAT_MS)) {
    speak(steerKey);
    lastSteerSpoken = steerKey;
    lastSteerRepeat = now;
  } else if (!steerKey) {
    lastSteerSpoken = null;
  }

  // Independent close-range obstacle callout (Vehicle A's own ultrasonic
  // sensor - fires even if Vehicle B is far away). Threshold is in meters;
  // 0.3m matches the tabletop-robot scale of the HC-SR04 (see backend
  // config.py ULTRASONIC_NEARBY_M).
  const a = state.vehicle_a || {};
  if (a.radar_target && a.radar_distance != null && a.radar_distance <= 0.3) {
    if (now - lastRadarSpoken > RADAR_REPEAT_MS) {
      speak("RADAR_OBSTACLE");
      lastRadarSpoken = now;
    }
  }

  // Fog severity - manual/demo input, announced once per change to HIGH.
  const fog = (state.environment || {}).fog_severity;
  if (fog === "HIGH" && lastFogSpoken !== "HIGH") {
    speak("FOG_HIGH");
    lastFogSpoken = "HIGH";
  } else if (fog !== "HIGH") {
    lastFogSpoken = fog;
  }
}
