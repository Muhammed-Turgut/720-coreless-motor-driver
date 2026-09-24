#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

// SI230DS kapısı (gate) bu pine gider.
// Boot sırasında motorun dönmemesi için kapı ile GND arasına 10k pull-down,
// ESP32 pini ile kapı arasına 220 ohm seri direnç koy.
static constexpr uint8_t MOTOR_PIN = 25;
static constexpr uint8_t PWM_CHANNEL = 0;
static constexpr uint32_t PWM_FREQ_HZ = 20000;
static constexpr uint8_t PWM_BITS = 10;
static constexpr uint32_t PWM_MAX = (1u << PWM_BITS) - 1u;

static constexpr char AP_SSID[] = "720-Motor";
static constexpr char AP_PASS[] = "motor720";

// Ev ağını kullanmak istersen doldur. Boş bırakırsan sadece yukarıdaki erişim noktası açılır.
static constexpr char WIFI_SSID[] = "";
static constexpr char WIFI_PASS[] = "";

static constexpr uint32_t RAMP_STEP_MS = 8;

WebServer server(80);

static int targetPercent = 0;
static int appliedPercent = 0;
static uint32_t lastRampMs = 0;

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>720 Motor</title>
<style>
  :root {
    color-scheme: dark;
    --bg: #0e1116;
    --card: #171c24;
    --line: #2a3340;
    --text: #e8eef6;
    --muted: #8b98a8;
    --accent: #3dff9a;
    --danger: #ff4d4d;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    min-height: 100vh;
    font-family: "Segoe UI", system-ui, sans-serif;
    background: radial-gradient(1200px 500px at 50% -10%, #1a2836, var(--bg));
    color: var(--text);
    display: grid;
    place-items: center;
    padding: 24px;
  }
  main {
    width: min(440px, 100%);
    background: var(--card);
    border: 1px solid var(--line);
    border-radius: 20px;
    padding: 28px 24px 24px;
    box-shadow: 0 20px 60px rgba(0,0,0,.35);
  }
  h1 { margin: 0 0 4px; font-size: 1.35rem; font-weight: 650; }
  p.sub { margin: 0 0 22px; color: var(--muted); font-size: .92rem; }
  .readout {
    font-variant-numeric: tabular-nums;
    font-size: 4.2rem;
    font-weight: 700;
    letter-spacing: -0.04em;
    line-height: 1;
    margin: 8px 0 6px;
  }
  .readout span { font-size: 1.4rem; color: var(--muted); margin-left: 4px; }
  input[type=range] {
    width: 100%;
    margin: 18px 0 8px;
    accent-color: var(--accent);
  }
  .presets { display: flex; gap: 8px; margin: 14px 0 18px; }
  button {
    flex: 1;
    border: 1px solid var(--line);
    background: #12171e;
    color: var(--text);
    border-radius: 12px;
    padding: 12px 0;
    font-size: 1rem;
    cursor: pointer;
  }
  button.stop {
    width: 100%;
    background: var(--danger);
    border-color: transparent;
    color: white;
    font-weight: 700;
    letter-spacing: .04em;
    padding: 16px;
  }
  #status { margin-top: 14px; color: var(--muted); font-size: .85rem; min-height: 1.2em; }
</style>
</head>
<body>
<main>
  <h1>720 Çekirdeksiz</h1>
  <p class="sub">SI230DS düşük taraf PWM · GPIO 25</p>
  <div class="readout" id="value">0<span>%</span></div>
  <input id="speed" type="range" min="0" max="100" value="0" step="1">
  <div class="presets">
    <button type="button" data-speed="25">25</button>
    <button type="button" data-speed="50">50</button>
    <button type="button" data-speed="75">75</button>
    <button type="button" data-speed="100">100</button>
  </div>
  <button class="stop" id="stop" type="button">DUR</button>
  <div id="status">Bağlanıyor…</div>
</main>
<script>
const slider = document.getElementById("speed");
const value = document.getElementById("value");
const status = document.getElementById("status");
let pending = null;

function show(percent) {
  value.innerHTML = percent + "<span>%</span>";
  slider.value = percent;
}

async function send(percent) {
  show(percent);
  if (pending) clearTimeout(pending);
  pending = setTimeout(async () => {
    try {
      const res = await fetch("/api/speed?value=" + percent, { method: "POST" });
      if (!res.ok) throw new Error("http");
      status.textContent = "GPIO 25 · " + percent + "%";
    } catch (e) {
      status.textContent = "Bağlantı koptu";
    }
  }, 40);
}

slider.addEventListener("input", () => send(Number(slider.value)));
document.getElementById("stop").addEventListener("click", () => send(0));
document.querySelectorAll("[data-speed]").forEach((btn) => {
  btn.addEventListener("click", () => send(Number(btn.dataset.speed)));
});

fetch("/api/status").then((r) => r.json()).then((data) => {
  show(data.speed);
  status.textContent = "Hazır · GPIO " + data.pin;
}).catch(() => { status.textContent = "Durum okunamadı"; });
</script>
</body>
</html>
)HTML";

static void applyDuty(int percent) {
  percent = constrain(percent, 0, 100);
  const uint32_t duty = (PWM_MAX * static_cast<uint32_t>(percent)) / 100u;
  ledcWrite(PWM_CHANNEL, duty);
}

static void setTarget(int percent) {
  targetPercent = constrain(percent, 0, 100);
}

static void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handleSpeed() {
  if (!server.hasArg("value")) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  setTarget(server.arg("value").toInt());
  server.send(200, "application/json", "{\"ok\":true,\"speed\":" + String(targetPercent) + "}");
}

static void handleStatus() {
  const String body = "{\"speed\":" + String(targetPercent) + ",\"pin\":" + String(MOTOR_PIN) + "}";
  server.send(200, "application/json", body);
}

static void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("Erisim noktasi: ");
  Serial.println(AP_SSID);
  Serial.print("Sifre: ");
  Serial.println(AP_PASS);
  Serial.print("Arayuz: http://");
  Serial.println(WiFi.softAPIP());
}

static void startWifi() {
  if (WIFI_SSID[0] == '\0') {
    startAccessPoint();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi baglaniyor");
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 12000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Arayuz: http://");
    Serial.println(WiFi.localIP());
    return;
  }

  Serial.println("Ev agina baglanamadi, erisim noktasi aciliyor.");
  startAccessPoint();
}

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);

  ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_BITS);
  ledcAttachPin(MOTOR_PIN, PWM_CHANNEL);
  applyDuty(0);

  startWifi();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/speed", HTTP_POST, handleSpeed);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.begin();

  Serial.println("Motor kapali. Hiz kaydirici 0-100.");
}

void loop() {
  server.handleClient();

  const uint32_t now = millis();
  if (now - lastRampMs < RAMP_STEP_MS || appliedPercent == targetPercent) {
    return;
  }
  lastRampMs = now;
  appliedPercent += (appliedPercent < targetPercent) ? 1 : -1;
  applyDuty(appliedPercent);
}
