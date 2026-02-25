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

## Agent 6 — Unit tests — 2026-02-25

Files created/modified:

- test/test_all.cpp (8 Unity tests: utils x6, config_apply_defaults x2)
- lib/ConfigManager/ConfigManager.cpp (config_apply_defaults moved outside NATIVE_TEST guard)
- src/utils.h (added stdint.h for uint32_t on native builds)

Build: pio test -e native → BLOCKED (no GCC/g++ in PATH on this Windows system)
Notes: Tests are discovered and structurally correct (file compilation attempted by PlatformIO).
To run: install MinGW-w64 from [winlibs.com](https://winlibs.com/), add bin/ to PATH, then pio test -e native.
PlatformIO v6.1 discovers test files at test/ root only (not subdirectories).
Merged via branch agent/6-unit-tests.

## Agent 7 — Build Verification — 2026-02-25

Files created/modified:

- none (clean pass, no fixes required)

Build: pio run -e d1_mini → PASS (SUCCESS, RAM 40.5%, Flash 38.3%)
Notes: Full integration build after Agent 6's changes (ConfigManager.cpp restructure,
utils.h stdint.h fix) compiled cleanly. Only warning is in upstream PubSubClient.cpp:523
(signed/unsigned comparison in publish_P) — not actionable. No source modifications needed.
Verified via branch agent/7-build-verify.

## Agent 8 — Compliance Review — 2026-02-25

Files created/modified:

- REVIEW.md (568 lines — full compliance checklist, 9 sections, every item cited to file:line)
- src/main.cpp (Deviation 1 fix: 300s deepSleep on portal timeout before reboot)

Build: pio run -e d1_mini → PASS (SUCCESS, RAM 40.5%, Flash 38.3%)
Notes: Two deviations found.
  Deviation 1 (FIXED): portal timeout path called ESP.restart() immediately;
    requirement specifies 300s deepSleep first. Fixed via portal_save_fired flag.
  Deviation 2 (LIBRARY LIMIT, not fixable): status MQTT publish uses QoS 0;
    requirement specifies QoS 1. PubSubClient publish() has no QoS parameter —
    documented in REVIEW.md §10.
Merged via branch agent/8-compliance-review.

## Agent 10 — utils.h + unit tests — 2026-02-25

Files created/modified:

- src/utils.h (added build_telemetry_topic, battery_status_str)
- test/test_all.cpp (added 6 new Unity tests)
- logs/agent10-utils-tests.md

Build: pio run -e d1_mini → PASS (RAM 40.5%, Flash 38.3%)
Tests: pio test -e native → PASS (14/14 tests — 8 existing + 6 new)
Notes: build_telemetry_topic() formats topics as "{root}/{device}/telemetry/{sub}" for
Iteration 2's new MQTT topic hierarchy. battery_status_str() returns "BAT_CRIT",
"BAT_LOW", or nullptr based on <= comparisons against thresholds. All 6 new tests
passed natively (GCC available in PATH). No existing tests were modified or broken.
Merged via branch agent/10-utils-tests.

## Agent 11 — main.cpp Iteration 2 — 2026-02-25

Files created/modified:

- src/main.cpp (Iteration 2: ADC read, telemetry topics, battery status priority, battery sleep, chained sleep)
- logs/agent11-main-iteration2.md

Build: pio run -e d1_mini → PASS (RAM 40.6%, Flash 38.4%)
Notes: Added BATTERY_ADC_SCALE (4.2f/1023.0f), SLEEP_MAGIC sentinel, SLEEP_MAX_S=4294 defines.
sleep_chained() helper writes remaining duration to RTC user memory before each segment sleep.
Chained sleep continuation check at setup() start resumes sleep without full publish cycle.
Step 5b reads analogRead(A0) * BATTERY_ADC_SCALE for battery voltage.
Topics use build_telemetry_topic() for temp/hum/volt; build_topic() unchanged for status.
Step 7 applies battery_status_str() priority (BAT_CRIT/BAT_LOW) before sensor OK/NOK.
Step 9b publishes battery voltage to telemetry/voltage (always, regardless of sensor state).
Step 13 selects sleep_s from battery thresholds (critical/low/normal) then calls sleep_chained().
Merged via branch agent/11-main-iteration2.
