# OtaWebUpdater: ESP32 local OTA + web updater

Minimal template for a Classic ESP32 Dev Module (ESP32-WROOM-32). It has no MQTT, no cloud and no third-party libraries.

- Wi-Fi manager with timeout, retry and status logging
- ArduinoOTA (Arduino IDE "Network ports")
- Browser updater at `http://<ip>/update` (upload a `.bin` from a phone)
- Non-blocking LED blink on GPIO 2 to prove `loop()` stays responsive

> The old "ESP32WebServer" library is now the built-in `WebServer` library in the ESP32 core. You install nothing extra.

---

## Step 1: Install the tooling (one time)

1. Install Arduino IDE 2.x.
2. Open **File > Preferences** and add this to *Additional boards manager URLs*:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Open **Tools > Board > Boards Manager**, search `esp32` and install **esp32 by Espressif Systems** (2.x or 3.x both work).
4. Select **Tools > Board > esp32 > ESP32 Dev Module**.

Libraries needed: **none to install**. `WiFi`, `ESPmDNS`, `ArduinoOTA`, `WebServer` and `Update` all ship with the core.

**Verify:** the board appears under Tools, and **File > Examples > ArduinoOTA > BasicOTA** is listed.

## Step 2: Configure the sketch

Open `OtaWebUpdater/OtaWebUpdater.ino` and edit the `USER CONFIG` block:

- `WIFI_SSID` and `WIFI_PASSWORD`: use a 2.4 GHz network, because the ESP32 has no 5 GHz radio
- `OTA_PASSWORD`: used by the Arduino IDE
- `WEB_USER` and `WEB_PASS`: used by the browser updater
- `HOSTNAME`: defaults to `esp32-ota`
- `LED_PIN`: defaults to GPIO 2, so change it if your board's LED is elsewhere

Change all the default passwords. Anyone on your LAN who knows them can flash the device.

**Verify:** nothing still says `YOUR_WIFI_...` or `change-me-...`.

## Step 3: First flash over USB (one time)

1. Plug in the USB cable and select the port under **Tools > Port**.
2. Use these settings:
   - Upload Speed: 921600 (use 115200 if uploads fail)
   - Partition Scheme: **Default 4MB with spiffs**. It has two app slots (OTA needs this).
3. Click **Upload**. If it stays on "Connecting...", hold the board's **BOOT** button until writing starts.
4. Open **Tools > Serial Monitor** at **115200 baud** and press **EN/RST**.

**Verify (serial output should look like this):**

```
=== OtaWebUpdater v1.0.0 ===
[WIFI] Connecting...
[WIFI] Connected: IP=192.168.1.50 RSSI=-58 dBm
[OTA] ArduinoOTA ready
[WEB] Updater at http://192.168.1.50/update
[APP] v1.0.0 up=5s ...
```

The LED should also blink at about 1 Hz.

| Symptom | Cause |
|---|---|
| `SSID_NOT_FOUND` | Wrong name, or the network is 5 GHz only |
| `CONNECT_FAILED (bad password?)` | Wrong password |
| It retries every 10 s | Normal. Fix the config and re-flash over USB. |

## Step 4: Check the web page from your phone

1. Connect the phone to the **same Wi-Fi network**.
2. Open `http://<IP from the serial monitor>/`, or `http://esp32-ota.local/` (mDNS can be flaky on Android, so use the IP if it fails).

**Verify:** the status page shows firmware `1.0.0`, uptime and heap.

## Step 5: Wireless update from the Arduino IDE

1. Change something visible, for example set `FW_VERSION` to `"1.0.1"` or `BLINK_MS` to `100`.
2. Put your computer on the same network and wait about 10 s.
3. Choose **Tools > Port**, then the entry under **Network ports**, such as `esp32-ota at 192.168.1.50`.
4. Click **Upload**. Enter `OTA_PASSWORD` when asked.

**Verify:** the IDE shows 100%. The serial monitor shows `[OTA] Done, rebooting`, then `v1.0.1`, and the LED rate changes.

If the Network port is missing:
- Check that the computer and the ESP32 are on the same subnet.
- Check that the firewall allows mDNS (UDP 5353) and the OTA port (UDP/TCP 3232).
- Restart the IDE.
- Guest or "client isolation" Wi-Fi networks block this.

## Step 6: Wireless update from a smartphone browser

1. In the Arduino IDE, choose **Sketch > Export Compiled Binary** (Ctrl+Alt+S). It writes `OtaWebUpdater.ino.bin` into the sketch folder. Use the one **without** `bootloader` or `partitions` in the name.
2. Move the `.bin` to the phone (cloud drive, AirDrop, USB or email).
3. On the phone, open `http://<ip>/update` and sign in with `WEB_USER` and `WEB_PASS`.
4. Tap the file picker, choose the `.bin` and tap **Upload & flash**.

**Verify:** the progress bar fills and the page says "Success. Rebooting". After about 10 s the status page `/` shows the new `FW_VERSION`. The serial monitor shows `[WEB] Upload OK`.

| Result | Meaning |
|---|---|
| `Update failed: Wrong Magic Byte` | You picked the wrong file (a bootloader or partitions `.bin`) |
| `Update failed: Not Enough Space` | Wrong partition scheme (it needs two OTA slots) |
| The old firmware keeps running | A failed update never overwrites the running slot, so retry |

## Step 7: Check that updates don't disturb the app

1. Keep the serial monitor open.
2. Confirm the `[APP] ... heartbeat` line appears every 5 s while the LED blinks.
3. Start any update. The blink may pause during the transfer, because the CPU is busy writing flash. It resumes after the reboot.

## Important: keep the updater in every build

Every image you upload must itself contain this OTA and web-updater code. If you flash a sketch without it, you lose wireless updates until you reconnect USB. Build your own application by adding code to `loop()` using `millis()` timing and no long `delay()` calls.

## Security notes

- ArduinoOTA uses your password, but the web updater uses HTTP Basic auth over plain HTTP. Use it on a trusted LAN only.
- Don't expose port 80 or 3232 to the internet.

## Status of this template

I wrote this without access to the ESP32 toolchain, so it has not been compiled or run on hardware. It targets the standard APIs of the Arduino-ESP32 2.x and 3.x cores. If compilation reports an error, send me the message and I'll fix it.
