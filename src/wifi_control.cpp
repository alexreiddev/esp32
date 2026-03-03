#include "wifi_control.h"
#include "lighting.h"
#include "sensor.h"
#include "state_machine.h"
#include <WiFi.h>
#include <WebServer.h>

static WebServer server(WEB_SERVER_PORT);
static bool autoMode = true;

// --- HTML Dashboard ---
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Adaptive Light</title>
<style>
  body { font-family: sans-serif; background: #1a1a2e; color: #eee; margin: 0; padding: 20px; }
  .card { background: #16213e; border-radius: 12px; padding: 20px; margin-bottom: 16px; }
  h1 { color: #e94560; margin-top: 0; }
  h2 { color: #0f3460; margin-top: 0; font-size: 1.1em; color: #a8a8b8; }
  .value { font-size: 2em; font-weight: bold; }
  .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }
  .color-preview { width: 60px; height: 60px; border-radius: 50%; display: inline-block; border: 3px solid #333; }
  input[type=color] { width: 60px; height: 40px; border: none; cursor: pointer; }
  button { background: #e94560; color: white; border: none; padding: 10px 24px;
           border-radius: 8px; font-size: 1em; cursor: pointer; margin: 4px; }
  button:hover { background: #c73650; }
  button.secondary { background: #0f3460; }
  button.secondary:hover { background: #1a4a8a; }
  .status { display: inline-block; padding: 4px 12px; border-radius: 20px; font-size: 0.85em; }
  .status.active { background: #2ecc71; color: #000; }
  .status.idle { background: #f39c12; color: #000; }
  .status.sleep { background: #e74c3c; }
</style>
</head>
<body>
<h1>Adaptive Presence Light</h1>

<div class="grid">
  <div class="card">
    <h2>Distance</h2>
    <div class="value" id="dist">--</div>
    <div>cm</div>
  </div>
  <div class="card">
    <h2>State</h2>
    <div id="state" class="value">--</div>
  </div>
</div>

<div class="card">
  <h2>Current Color</h2>
  <div class="color-preview" id="colorPreview"></div>
  <span id="colorText" style="margin-left:12px">--</span>
</div>

<div class="card">
  <h2>Ambient Light</h2>
  <div class="value" id="ambient">--</div>
</div>

<div class="card">
  <h2>Controls</h2>
  <div style="margin-bottom:12px">
    <button id="btnAuto" onclick="setMode('auto')">Auto Mode</button>
    <button id="btnManual" class="secondary" onclick="setMode('manual')">Manual</button>
  </div>
  <div id="manualControls" style="display:none">
    <label>Pick color: <input type="color" id="colorPicker" value="#ff0000"></label>
    <button onclick="sendColor()">Apply</button>
  </div>
</div>

<script>
function fetchData() {
  fetch('/api/status').then(r => r.json()).then(d => {
    document.getElementById('dist').textContent = d.distance.toFixed(1);
    var states = ['ACTIVE','IDLE','SLEEP'];
    var cls = ['active','idle','sleep'];
    document.getElementById('state').innerHTML =
      '<span class="status ' + cls[d.state] + '">' + states[d.state] + '</span>';
    var r = Math.round(d.color_r), g = Math.round(d.color_g), b = Math.round(d.color_b);
    var hex = '#' + [r,g,b].map(x => x.toString(16).padStart(2,'0')).join('');
    document.getElementById('colorPreview').style.background = hex;
    document.getElementById('colorText').textContent = 'R:'+r+' G:'+g+' B:'+b;
    document.getElementById('ambient').textContent = (d.ambient * 100).toFixed(0) + '%';
    if (d.auto_mode) {
      document.getElementById('btnAuto').style.background = '#2ecc71';
      document.getElementById('btnManual').style.background = '#0f3460';
      document.getElementById('manualControls').style.display = 'none';
    } else {
      document.getElementById('btnAuto').style.background = '#0f3460';
      document.getElementById('btnManual').style.background = '#e94560';
      document.getElementById('manualControls').style.display = 'block';
    }
  }).catch(e => {});
}
function setMode(m) {
  fetch('/api/mode?mode=' + m);
  setTimeout(fetchData, 200);
}
function sendColor() {
  var hex = document.getElementById('colorPicker').value;
  var r = parseInt(hex.substr(1,2),16);
  var g = parseInt(hex.substr(3,2),16);
  var b = parseInt(hex.substr(5,2),16);
  fetch('/api/color?r='+r+'&g='+g+'&b='+b);
  setTimeout(fetchData, 200);
}
setInterval(fetchData, 500);
fetchData();
</script>
</body>
</html>
)rawliteral";

static void handleRoot() {
    server.send(200, "text/html", INDEX_HTML);
}

static void handleStatus() {
    Color c = getCurrentColor();
    float dist = getSmoothedDistance();
    float ambient = getAmbientBrightness();
    SystemState state = getCurrentState();

    String json = "{";
    json += "\"distance\":" + String(dist, 1) + ",";
    json += "\"state\":" + String((int)state) + ",";
    json += "\"color_r\":" + String(c.r, 1) + ",";
    json += "\"color_g\":" + String(c.g, 1) + ",";
    json += "\"color_b\":" + String(c.b, 1) + ",";
    json += "\"ambient\":" + String(ambient, 3) + ",";
    json += "\"auto_mode\":" + String(autoMode ? "true" : "false");
    json += "}";

    server.send(200, "application/json", json);
}

static void handleMode() {
    if (server.hasArg("mode")) {
        String mode = server.arg("mode");
        if (mode == "auto") {
            autoMode = true;
            clearOverride();
        } else if (mode == "manual") {
            autoMode = false;
        }
    }
    server.send(200, "text/plain", "OK");
}

static void handleColor() {
    if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
        uint8_t r = (uint8_t)server.arg("r").toInt();
        uint8_t g = (uint8_t)server.arg("g").toInt();
        uint8_t b = (uint8_t)server.arg("b").toInt();
        autoMode = false;
        setOverrideColor(r, g, b);
    }
    server.send(200, "text/plain", "OK");
}

void wifiInit() {
    if (WIFI_AP_MODE) {
        WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
    } else {
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        Serial.print("Connecting to WiFi");
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 40) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        Serial.println();
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("IP: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("WiFi connection failed, falling back to AP mode");
            WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
            Serial.print("AP IP: ");
            Serial.println(WiFi.softAPIP());
        }
    }

    server.on("/", handleRoot);
    server.on("/api/status", handleStatus);
    server.on("/api/mode", handleMode);
    server.on("/api/color", handleColor);
    server.begin();
    Serial.println("Web server started");
}

void wifiUpdate() {
    server.handleClient();
}
