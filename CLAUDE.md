# Environmental Sensor V3

ESP8266 (Wemos D1 Mini clone) IoT sensor using PlatformIO + Arduino framework.

@REQUIREMENTS.md

## Hardware

- Board: Wemos D1 Mini clone (ESP8266) — PlatformIO board ID: `d1_mini`
- Sensor: DHT11 on pin D5 (GPIO14)
- Flash filesystem: LittleFS — config stored at `/config.json` (on-device path; `uploadfs` strips the `data/` prefix from the PC source path `data/config.json`)
- **Deep sleep wiring**: RST must be connected to D0 (GPIO16) — without this wire the device sleeps forever and never wakes

## PlatformIO Configuration

Minimum `platformio.ini` for this project:

```ini
[env:d1_mini]
platform = espressif8266
board = d1_mini
framework = arduino
board_build.filesystem = littlefs
monitor_speed = 115200

lib_deps =
    bblanchon/ArduinoJson @ ^6.21.0
    adafruit/DHT sensor library @ ^1.4.6
    knolleary/PubSubClient @ ^2.8.0
    tzapu/WiFiManager @ ^2.0.17
```

`board_build.filesystem = littlefs` is critical — omitting it causes `uploadfs` to use SPIFFS
and the firmware will fail to mount LittleFS at runtime.

LittleFS is part of the ESP8266 Arduino core — no separate `lib_deps` entry needed.
ArduinoJson is pinned to v6; do not upgrade to v7 (different API).

## Build & Flash

```bash
pio run                          # build
pio run --target upload          # build + flash firmware
pio run --target uploadfs        # upload LittleFS data/ folder (config.json)
pio device monitor               # open serial monitor (115200 baud)
pio run --target clean           # clean build artifacts
```

`data/config.json` is gitignored (contains credentials). Use `data/config.example.json` as
the committed template. Do not re-add `data/config.json` to git. Configure the device via the
WiFiManager portal — it writes directly to the device flash and never touches the PC file.

## Code Style

- Language: C++ / Arduino framework, written in **C-style conventions**
- Naming: `snake_case` for variables and functions, `UPPER_CASE` for constants and macros
- Indentation: **4 spaces** (no tabs)
- Dynamic memory allocation (`String`, `new`, `malloc`) is acceptable — deep sleep resets
  the heap on every cycle; ESP8266 has 80KB DRAM total, with ~40KB typically free after the WiFi stack
- Use `Serial.begin(115200)` in `setup()`; print loaded config values and each phase outcome
  (WiFi connected/failed, sensor OK/NOK, MQTT published/failed) — aids debugging without hardware

## Constraints & Gotchas

- Mount LittleFS before any file access; always check the return value
- DHT11 minimum sampling interval is 1s — use `delay(1000)` between reads
- Device name must always be derived from `ESP.getChipId()` — never hardcoded or configured
- Config values are native JSON types — use ArduinoJson typed accessors (`as<int>()`, `as<float>()`, `as<bool>()`)
- WiFiManager portal inputs are always `char*` strings — parse to native types before writing to config.json
- After `client.publish()`, call `client.loop()` and wait ~100ms before `client.disconnect()` — PubSubClient buffers outgoing messages and requires `loop()` to flush them
- The 60s error LED loop must call `delay()` internally (not just GPIO toggling) — the ESP8266 software watchdog resets the device after ~3s of code that does not yield
- If `wifi.reset: true` is set in `data/config.json` on the PC and `uploadfs` is run, remember to reset it to `false` in the PC copy after the portal has reconfigured the device
- WiFiManager portal requires `wifiManager.setConfigPortalBlocking(false)` before `autoConnect()` / `startConfigPortal()` to enable the non-blocking `process()` loop for LED toggling; also call `wifiManager.setConfigPortalTimeout(seconds)` — without it the portal stays open forever
- LWT (Last Will and Testament) is not a separate call — it is passed as arguments to `client.connect(clientId, user, pass, willTopic, willQoS, willRetain, willMessage)`
- If `sleep.normal_s` is greater than 4294 (the ESP8266 hardware deep sleep limit), print a Serial warning; Iteration 1 does not implement chained sleep, so the device will wake earlier than configured
- `ESP.deepSleep()` takes microseconds as `uint64_t` — use `(uint64_t)sleep_s * 1000000ULL`, not `sleep_s * 1000000`; the latter silently overflows on `int` for values above ~71 minutes
- WiFi connection strategy — three distinct paths:
  - **Scenario 1 (no saved credentials)**: detect with `WiFi.SSID().length() == 0`; call `wifiManager.setConfigPortalTimeout(600)` then `wifiManager.autoConnect()` — WiFiManager opens the portal automatically; timeout is 10 min
  - **Scenarios 2 & 3 (config failure / wifi.reset)**: call `wifiManager.setConfigPortalTimeout(300)` then `wifiManager.startConfigPortal()` — portal opens immediately; timeout is 5 min
  - **Normal operation (saved credentials exist)**: call `WiFi.begin()` with no arguments — uses ESP8266 saved flash credentials from the last `autoConnect()` session; poll `WiFi.status()` with a `millis()` deadline; do NOT use `autoConnect()` here as it would open the portal on failure
- Register `wifiManager.setSaveConfigCallback(fn)` before opening the portal — without this the firmware receives no signal that the portal saved and will never write the custom parameters (MQTT, sleep, battery) to `config.json`
