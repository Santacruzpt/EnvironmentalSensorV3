# Agent Status Log

## Agent 1 — Interfaces + Config + platformio.ini — 2026-02-25

Files created:

- lib/ConfigManager/ConfigManager.h
- lib/ConfigManager/ConfigManager.cpp
- lib/WifiPortalManager/WifiPortalManager.h
- lib/MqttClient/MqttClient.h
- lib/LedIndicator/LedIndicator.h
- lib/DhtSensor/DhtSensor.h
- src/utils.h
- TASKS.md

Files modified:

- platformio.ini (added [env:native])

Build: pio run -e d1_mini → PASS (SUCCESS, 68s, RAM 34.2%, Flash 24.9%)
Tests: n/a (no test files yet)
Notes: All module interfaces are locked. Agents 2-4 must not modify .h signatures.
       A minimal placeholder src/main.cpp (setup/loop stubs) was added to satisfy
       PlatformIO's requirement for at least one source file in src/. Agent 5 must
       replace this file with the full implementation.

## Agent 2 — WifiPortalManager.cpp — 2026-02-25

Files created:

- lib/WifiPortalManager/WifiPortalManager.cpp

Build: pio run -e d1_mini → PASS (verified by orchestrator)
Notes: wifi_connect uses WiFi.begin() with no args (saved credentials). Portal loop uses
mgr.getConfigPortalActive(). PORTAL_SAVED detected by WiFi.status() == WL_CONNECTED
after loop exits. Save callback must be registered by caller (main.cpp) before
calling wifi_open_portal.

## Agent 3 — MqttClient.cpp + DhtSensor.cpp — 2026-02-25

Files created:

- lib/MqttClient/MqttClient.cpp
- lib/DhtSensor/DhtSensor.cpp

Build: pio run -e d1_mini → PASS (verified by orchestrator)
Notes: PubSubClient publish() has no QoS parameter — all publishes use QoS 0 with
retain=true. mqtt_connect uses static WiFiClient inside function. DHT delay(1000)
between reads except after the last read. LWT embedded in client.connect() 8-arg form.

## Agent 4 — LedIndicator.cpp — 2026-02-25

Files created:

- lib/LedIndicator/LedIndicator.cpp

Build: pio run -e d1_mini → PASS (verified by orchestrator)
Notes: All led_update_* use static phase variables; GPIO written every call (harmless).
led_error_blocking uses delay() to feed ESP8266 watchdog. Active LOW: LOW=on, HIGH=off.

## Agent 5 — src/main.cpp — 2026-02-25

Files created/modified:

- src/main.cpp (replaced placeholder)

Build: pio run -e d1_mini → PASS (SUCCESS, 25s, RAM 40.5%, Flash 38.3%)
Notes: WiFi and MQTT connect loops inlined in main.cpp for LED integration (millis()-based
LED requires access to the polling loop). open_portal_and_reboot() helper centralises
parameter setup; never returns. deepSleep uses (uint64_t) cast. Static WiFiClient declared
in setup() for MQTT (avoids new/malloc). config_save(cfg) with wifi_reset=false written
before WiFi.disconnect() in scenario 3. Sensor reads inlined with LED updates in inter-read
gaps. Serial warning emitted if sleep_normal_s > 4294.
