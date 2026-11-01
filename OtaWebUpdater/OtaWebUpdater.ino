/*
 * OtaWebUpdater - minimal Wi-Fi + ArduinoOTA + browser-upload firmware updater
 * Target: Classic ESP32 Dev Module (ESP32-WROOM-32), Arduino-ESP32 core 2.x or 3.x
 *
 * Features
 *   1. Wi-Fi connection manager (timeout, retry, auto-reconnect, status logging)
 *   2. ArduinoOTA  -> flash from Arduino IDE "Network ports"
 *   3. Web updater -> http://<device-ip>/update  (upload a compiled .bin from any browser)
 *   4. Non-blocking sample task (LED blink) proving loop() stays responsive
 *
 * No external libraries needed: everything below ships with the ESP32 Arduino core.
 * (The old "ESP32WebServer" library is now the built-in "WebServer" library.)
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <Update.h>

// ============================ USER CONFIG ===================================
static const char *WIFI_SSID     = "YOUR_WIFI_SSID";
static const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

static const char *HOSTNAME      = "esp32-ota";      // reachable as esp32-ota.local
static const char *OTA_PASSWORD  = "change-me-ota";  // ArduinoOTA password
static const char *WEB_USER      = "admin";          // web updater login
static const char *WEB_PASS      = "change-me-web";

static const char *FW_VERSION    = "1.0.0";          // bump this to verify an update landed

static const uint8_t  LED_PIN      = 2;              // onboard LED on most ESP32 DevKit boards
static const uint32_t BLINK_MS     = 500;
static const uint32_t WIFI_TRY_MS  = 20000;          // max wait per connection attempt
static const uint32_t WIFI_RETRY_MS = 10000;         // pause between reconnect attempts
// ============================================================================

WebServer server(80);

static bool     otaStarted   = false;
static bool     webStarted   = false;
static bool     otaBusy      = false;   // true while any update is in progress
static uint32_t restartAtMs  = 0;       // non-zero => reboot at this millis()

// ------------------------------- Logging -------------------------------------
static void logLine(const char *tag, const char *msg) {
  Serial.printf("[%8lu] [%s] %s\n", (unsigned long)millis(), tag, msg);
}

static const char *wifiStatusName(wl_status_t s) {
  switch (s) {
    case WL_NO_SHIELD:       return "NO_SHIELD";
    case WL_IDLE_STATUS:     return "IDLE";
    case WL_NO_SSID_AVAIL:   return "SSID_NOT_FOUND";
    case WL_SCAN_COMPLETED:  return "SCAN_DONE";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT_FAILED (bad password?)";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "UNKNOWN";
  }
}

// --------------------------- Wi-Fi connection manager ------------------------
// Blocking attempt, used only in setup(). Returns true on success.
static bool wifiConnectBlocking(uint32_t timeoutMs) {
  logLine("WIFI", "Connecting...");
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);              // don't wear flash re-writing credentials
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t start = millis();
  wl_status_t last = WL_NO_SHIELD;
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    wl_status_t s = WiFi.status();
    if (s != last) {
      Serial.printf("[%8lu] [WIFI] status: %s\n", (unsigned long)millis(), wifiStatusName(s));
      last = s;
    }
    if (s == WL_CONNECT_FAILED || s == WL_NO_SSID_AVAIL) {
      // Fatal for this attempt; retrying immediately will not help.
      break;
    }
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[%8lu] [WIFI] Connected: IP=%s RSSI=%d dBm\n", (unsigned long)millis(),
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    return true;
  }
  Serial.printf("[%8lu] [WIFI] FAILED: %s\n", (unsigned long)millis(), wifiStatusName(WiFi.status()));
  return false;
}

// Non-blocking supervisor, called every loop().
static void wifiSupervise() {
  static uint32_t lastAttempt = 0;
  static bool wasConnected = false;

  bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected && !wasConnected) {
    Serial.printf("[%8lu] [WIFI] (Re)connected: IP=%s\n", (unsigned long)millis(),
                  WiFi.localIP().toString().c_str());
  } else if (!connected && wasConnected) {
    logLine("WIFI", "Connection lost");
  }
  wasConnected = connected;

  if (!connected && !otaBusy && millis() - lastAttempt >= WIFI_RETRY_MS) {
    lastAttempt = millis();
    logLine("WIFI", "Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

// ------------------------------- ArduinoOTA ----------------------------------
static void otaSetup() {
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    otaBusy = true;
    logLine("OTA", ArduinoOTA.getCommand() == U_FLASH ? "Start: firmware" : "Start: filesystem");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    static uint8_t lastPct = 255;
    uint8_t pct = (done * 100U) / total;
    if (pct != lastPct && pct % 10 == 0) {
      Serial.printf("[%8lu] [OTA] %u%%\n", (unsigned long)millis(), pct);
      lastPct = pct;
    }
  });
  ArduinoOTA.onEnd([]() { logLine("OTA", "Done, rebooting"); });
  ArduinoOTA.onError([](ota_error_t e) {
    otaBusy = false;
    const char *m = "Unknown error";
    switch (e) {
      case OTA_AUTH_ERROR:    m = "Auth failed (wrong OTA password)"; break;
      case OTA_BEGIN_ERROR:   m = "Begin failed (partition/space?)";  break;
      case OTA_CONNECT_ERROR: m = "Connect failed";                   break;
      case OTA_RECEIVE_ERROR: m = "Receive failed";                   break;
      case OTA_END_ERROR:     m = "End failed (bad image?)";          break;
    }
    logLine("OTA", m);
  });

  ArduinoOTA.begin();   // also starts mDNS responder for HOSTNAME.local
  otaStarted = true;
  logLine("OTA", "ArduinoOTA ready");
}

// ------------------------------- Web updater ---------------------------------
static const char PAGE_HEAD[] PROGMEM =
  "<!DOCTYPE html><html><head><meta charset='utf-8'>"
  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
  "<title>ESP32 Updater</title><style>"
  "body{font-family:system-ui,sans-serif;max-width:420px;margin:2em auto;padding:0 1em}"
  "h1{font-size:1.3em}input[type=file]{width:100%;margin:1em 0}"
  "button{width:100%;padding:.9em;font-size:1em;border:0;border-radius:6px;"
  "background:#0a7;color:#fff}button:disabled{background:#999}"
  "progress{width:100%;height:1.2em;margin-top:1em}.m{color:#555;font-size:.9em}"
  "</style></head><body>";

static const char PAGE_UPDATE[] PROGMEM =
  "<h1>Firmware update</h1>"
  "<form id='f'><input type='file' name='fw' accept='.bin' required>"
  "<button id='b' type='submit'>Upload &amp; flash</button>"
  "<progress id='p' value='0' max='100' hidden></progress>"
  "<p id='s' class='m'></p></form>"
  "<script>"
  "f.onsubmit=function(e){e.preventDefault();var d=new FormData(f),x=new XMLHttpRequest();"
  "b.disabled=true;p.hidden=false;s.textContent='Uploading...';"
  "x.upload.onprogress=function(v){if(v.lengthComputable)p.value=v.loaded/v.total*100};"
  "x.onload=function(){s.textContent=x.status==200?'Success. Rebooting - reload in ~10s.':"
  "'Failed: '+x.responseText;if(x.status!=200)b.disabled=false};"
  "x.onerror=function(){s.textContent='Connection error';b.disabled=false};"
  "x.open('POST','/update');x.send(d)};"
  "</script>";

static bool webAuthOk() {
  if (server.authenticate(WEB_USER, WEB_PASS)) return true;
  server.requestAuthentication();
  return false;
}

static void handleRoot() {
  String html = FPSTR(PAGE_HEAD);
  html += "<h1>ESP32 status</h1><p class='m'>Firmware: ";
  html += FW_VERSION;
  html += "<br>Host: ";
  html += HOSTNAME;
  html += "<br>IP: " + WiFi.localIP().toString();
  html += "<br>RSSI: " + String(WiFi.RSSI()) + " dBm";
  html += "<br>Uptime: " + String(millis() / 1000) + " s";
  html += "<br>Free heap: " + String(ESP.getFreeHeap()) + " B</p>";
  html += "<p><a href='/update'>Go to firmware update</a></p></body></html>";
  server.send(200, "text/html", html);
}

static void handleUpdatePage() {
  if (!webAuthOk()) return;
  String html = FPSTR(PAGE_HEAD);
  html += FPSTR(PAGE_UPDATE);
  html += "<p class='m'>Current version: ";
  html += FW_VERSION;
  html += "</p></body></html>";
  server.send(200, "text/html", html);
}

// Runs after the upload has been fully received.
static void handleUpdateDone() {
  if (!webAuthOk()) return;
  if (Update.hasError()) {
    server.send(500, "text/plain", String("Update failed: ") + Update.errorString());
    otaBusy = false;
  } else {
    server.send(200, "text/plain", "OK");
    restartAtMs = millis() + 1000;   // let the HTTP response flush, then reboot from loop()
  }
}

// Runs for every chunk of the uploaded file.
static void handleUpdateUpload() {
  static bool authorized = false;
  HTTPUpload &up = server.upload();

  if (up.status == UPLOAD_FILE_START) {
    authorized = server.authenticate(WEB_USER, WEB_PASS);
    if (!authorized) { logLine("WEB", "Upload rejected: bad credentials"); return; }
    otaBusy = true;
    Serial.printf("[%8lu] [WEB] Upload start: %s\n", (unsigned long)millis(), up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (!authorized) {
    return;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("[%8lu] [WEB] Upload OK: %u bytes\n", (unsigned long)millis(), (unsigned)up.totalSize);
    } else {
      Update.printError(Serial);
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    otaBusy = false;
    logLine("WEB", "Upload aborted");
  }
}

static void webSetup() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
  webStarted = true;
  Serial.printf("[%8lu] [WEB] Updater at http://%s/update\n", (unsigned long)millis(),
                WiFi.localIP().toString().c_str());
}

// ------------------------- Sample non-blocking task --------------------------
static void blinkTask() {
  static uint32_t last = 0;
  static bool on = false;
  if (millis() - last >= BLINK_MS) {
    last = millis();
    on = !on;
    digitalWrite(LED_PIN, on);
  }
}

static void heartbeatTask() {
  static uint32_t last = 0;
  if (millis() - last >= 5000) {
    last = millis();
    Serial.printf("[%8lu] [APP] v%s up=%lus heap=%u wifi=%s\n", (unsigned long)millis(), FW_VERSION,
                  (unsigned long)(millis() / 1000), (unsigned)ESP.getFreeHeap(),
                  wifiStatusName(WiFi.status()));
  }
}

// ----------------------------------- Arduino ---------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== OtaWebUpdater v%s ===\n", FW_VERSION);
  pinMode(LED_PIN, OUTPUT);

  // Try once at boot; if it fails we keep running and loop() keeps retrying.
  wifiConnectBlocking(WIFI_TRY_MS);
}

void loop() {
  wifiSupervise();

  // Start network services the first time Wi-Fi is up.
  if (WiFi.status() == WL_CONNECTED) {
    if (!otaStarted) otaSetup();
    if (!webStarted) webSetup();
    ArduinoOTA.handle();
    server.handleClient();
  }

  if (restartAtMs && (int32_t)(millis() - restartAtMs) >= 0) {
    logLine("SYS", "Restarting after web update");
    ESP.restart();
  }

  blinkTask();       // keep working; never use delay() here
  heartbeatTask();
}
