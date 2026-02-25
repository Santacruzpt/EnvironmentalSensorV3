# Functional Requirements

## Iteration 1 — Environmental Sensor `[CURRENT]`

### Config

- Stored in LittleFS at `/config.json` (PC source: `data/config.json` — `uploadfs` strips the `data/` prefix)
- Loaded at startup; all runtime behaviour driven by config values
- Config is writable by firmware during portal save, and to clear `wifi.reset` before opening the portal in scenario 3 (see Configuration Portal)
- `data/config.json` is gitignored because it contains credentials; use `data/config.example.json` as the committed template
- `uploadfs` uploads the entire `data/` folder, so `config.example.json` will be present on-device alongside `config.json` — firmware loads only `config.json` and ignores any other files
- Config schema (native JSON types — no string coercion):

```json
{
    "wifi": {
        "reset": false
    },
    "mqtt": {
        "server": "",
        "port": 1883,
        "topic_root": "devices",
        "username": "",
        "password": ""
    },
    "sleep": {
        "normal_s": 60,
        "low_battery_s": 300,
        "critical_battery_s": 86400
    },
    "battery": {
        "low_v": 3.5,
        "critical_v": 3.2
    }
}
```

Note: `sleep.low_battery_s`, `sleep.critical_battery_s`, `battery.low_v`, `battery.critical_v` are
loaded but unused in Iteration 1 — reserved for Iteration 2.

MQTT credentials are stored in plaintext in LittleFS flash. This is the standard ESP8266
limitation — there is no hardware security element on this board. Acceptable for home IoT;
credentials must never appear in source code or be committed to git.

**Partial config handling**: if `config.json` is valid JSON but has missing keys, use hardcoded
defaults for the missing keys and continue without opening the portal. Only open the portal on
LittleFS mount failure, `config.json` not found, or invalid JSON.

Hardcoded defaults. For keys that appear in the Configuration Portal parameter table (below), these values must match the Default column. `wifi.reset` has a hardcoded default but is intentionally excluded from the portal (see Configuration Portal section):

| Config key | Default |
| --- | --- |
| `wifi.reset` | `false` |
| `mqtt.server` | `""` |
| `mqtt.port` | `1883` |
| `mqtt.topic_root` | `"devices"` |
| `mqtt.username` | `""` |
| `mqtt.password` | `""` |
| `sleep.normal_s` | `60` |
| `sleep.low_battery_s` | `300` |
| `sleep.critical_battery_s` | `86400` |
| `battery.low_v` | `3.5` |
| `battery.critical_v` | `3.40` |

### Configuration Portal

WiFiManager portal opens in three scenarios:

1. **First boot**: no saved WiFi credentials — portal opens with 10-minute timeout
2. **Config load failure**: LittleFS mount fails, `config.json` is not found, or `config.json` is invalid JSON — portal opens with 5-minute timeout
3. **`wifi.reset: true` in config**: saved credentials cleared, portal opens with 5-minute timeout. Write `wifi.reset: false` to `config.json` on-device **before** opening the portal — if the portal times out without saving, the device must not reopen the portal on the next wake.

If the portal times out without being configured: enter deep sleep for 300s (hardcoded fallback), then reboot into normal startup. For scenarios 1 and 2 (no creds / no config) the portal will likely reopen; for scenario 3 (wifi.reset) the device will attempt WiFi with its existing saved credentials.

Portal AP name: `EnvSensor-{chip_id}` (e.g., `EnvSensor-a1b2c3`) — identifies the device when
multiple units are deployed.

Portal inputs are HTML text fields — all values arrive as `char*` strings and must be parsed to
their native types (int, float, bool) before writing to `config.json`. This applies even to
numeric and boolean fields.

The MQTT password field must render as `<input type="password">` (masked). Pass the HTML type
attribute as a custom argument when creating the `WiFiManagerParameter`.

Portal timeouts must be set explicitly via `wifiManager.setConfigPortalTimeout()` before
opening the portal — without this call the portal stays open indefinitely.

The portal LED pattern (1s on / 1s off) requires non-blocking portal mode. Call
`wifiManager.setConfigPortalBlocking(false)` before `autoConnect()` or
`startConfigPortal()`, then loop calling `wifiManager.process()` with millis()-based LED
toggling until the portal times out or saves. `setAPCallback()` fires once when the AP
starts and cannot drive a repeating pattern on its own.

`wifi.reset` is intentionally excluded from portal parameters — it is a firmware control flag
triggered by uploading a modified `config.json`, not a user-configurable setting.

All other `config.json` fields must be presented as custom parameters.
Each `WiFiManagerParameter` requires a fixed buffer length allocated at declaration time:

| Parameter | Config key | Default | Buffer |
| --- | --- | --- | --- |
| MQTT Server | `mqtt.server` | _(empty)_ | 64 |
| MQTT Port | `mqtt.port` | 1883 | 8 |
| MQTT Username | `mqtt.username` | _(empty)_ | 64 |
| MQTT Password | `mqtt.password` | _(empty)_ | 64 |
| MQTT Topic Root | `mqtt.topic_root` | devices | 64 |
| Sleep Normal (s) | `sleep.normal_s` | 60 | 8 |
| Sleep Low Battery (s) | `sleep.low_battery_s` | 300 | 8 |
| Sleep Critical Battery (s) | `sleep.critical_battery_s` | 86400 | 8 |
| Battery Low Voltage | `battery.low_v` | 3.5 | 8 |
| Battery Critical Voltage | `battery.critical_v` | 3.40 | 8 |

On portal save: parse all string inputs to native types; write complete `config.json`;
set `wifi.reset` to `false`; reboot.

### Device Identity

- Device name derived from chip ID at runtime: `ESP.getChipId()` → formatted as `esp-{hex6}`
- Never configured manually; guarantees uniqueness across devices running the same firmware
- Used as the MQTT client ID and embedded in all MQTT topic paths

### MQTT Topics

- Constructed in code as `{mqtt.topic_root}/esp-{chip_id}/{subtopic}`
- Example with `topic_root = "devices"` and chip ID `a1b2c3`:
  - `devices/esp-a1b2c3/temperature`
  - `devices/esp-a1b2c3/humidity`
  - `devices/esp-a1b2c3/status`

Individual topics are used (one per measurement) rather than a single JSON payload because:

- Home Assistant sensor configuration is direct — no `value_template` needed
- Consumers can subscribe selectively to only the measurements they need
- Retain values are independent — if one read fails, the other retains its last value

#### Measurement Payloads

Plain string values, published to individual topics:

- `…/temperature` → `"22"` — degrees Celsius, integer string (no decimal places), retain true, QoS 0
- `…/humidity` → `"65"` — relative humidity %, integer string (no decimal places), retain true, QoS 0

Note: DHT11 has 1°C / 1% RH resolution and always returns integer values — publishing without
decimal places accurately reflects sensor resolution.

QoS 0 with retain=true means the broker stores and re-serves the last value, but if a publish
packet is lost in transit the retained value goes stale without either side knowing. This is
acceptable for a home LAN where packet loss is negligible; use QoS 1 if operating over a
less-reliable network.

#### Status Payload

Plain string, published to `…/status`:

- `"OK"` — sensor read and publish succeeded
- `"NOK"` — sensor read failed; measurements not published
- `"OFFLINE"` — Last Will and Testament; published automatically by broker on unexpected disconnect

#### MQTT Settings

- **Client ID**: `esp-{chip_id}`
- **Authentication**: username and password from `mqtt.username` / `mqtt.password` in config
- **QoS**: 0 for measurements; 1 for status
- **Retain**: true for temperature, humidity, and status
- **Keep-alive**: 60s — set explicitly via `client.setKeepAlive(60)` to outlast the maximum active cycle (~36s)
- **Connect timeout**: 5s per attempt — enforced by the retry loop with a `millis()` deadline; `client.setSocketTimeout()` controls read/write timeouts on an already-open socket, not the TCP connect timeout
- **LWT**: topic `…/status`, payload `"OFFLINE"`, QoS 1, retain true
- After publishing, call `client.loop()` at least once and wait ~100ms before disconnecting — PubSubClient buffers outgoing messages and requires `loop()` to flush them
- Before entering deep sleep, explicitly disconnect from MQTT broker to avoid triggering LWT on normal sleep cycles

### Sensor

- Device: DHT11 on pin D5 (GPIO14)
- Take 3 readings, 1s apart (~2–3s total); discard NaN/error results; publish the average of valid reads
- If all 3 reads fail, publish status "NOK" and skip measurements

### Connection Retries

- Both WiFi and MQTT: **3 attempts total** (1 initial + 2 retries), **2s fixed delay** between attempts
- WiFi: **10s timeout per attempt** before declaring failure
- MQTT: **5s timeout per attempt** — enforced by the retry loop with a `millis()` deadline
- Exponential backoff is not used — fixed delay is appropriate for a local network and gives a predictable power budget
- **WiFi connection** in normal operation (saved credentials exist) uses `WiFi.begin()` directly in a retry loop — **not** `autoConnect()`. Using `autoConnect()` here would open the portal on connection failure, violating the rule below.
- **WiFi failure** (all attempts exhausted): show error LED for 60s, then deep sleep for `sleep.normal_s`; do not open portal (portal is reserved for first boot and config failure)
- **MQTT failure** (all attempts exhausted): show error LED for 60s, then deep sleep for `sleep.normal_s`

### LED Indicators

- Built-in LED (`LED_BUILTIN`, D4/GPIO2, active LOW) used to signal operational phase
- WiFi and error phases must use `millis()`-based non-blocking toggling (operations are blocking)
- Portal LED must be driven from the firmware's own process loop (see Configuration Portal section)

| Phase | Pattern |
| --- | --- |
| Config load + WiFi connect | 0.5s on / 0.5s off, repeating |
| DHT11 sensor reads | 0.5s on / 0.5s off / 0.5s on / 1s off, repeating |
| MQTT connect + publish | 0.5s on / 0.5s off / 1s on, repeating |
| Portal active (WiFiManager) | 1s on / 1s off, repeating |
| Error (WiFi fail / MQTT fail) | 100ms on / 100ms off for 60s, then deep sleep |

### Publish Flow

1. Load config from LittleFS — on mount failure, missing file, or invalid JSON: open Configuration Portal; on missing keys only: use hardcoded defaults and continue
2. If `wifi.reset: true`, write `wifi.reset: false` to config.json on-device, clear WiFi credentials, and open Configuration Portal (see Configuration Portal section — scenario 3)
3. If no saved WiFi credentials: open Configuration Portal with 10-minute timeout (scenario 1 — first boot or credentials cleared); after portal save or timeout, reboot
4. Connect to WiFi — 3 attempts, 10s each **(LED: 0.5s blink)**; on failure: error LED 60s → LED off → deep sleep
5. Read sensor — 3 reads, 1s apart **(LED: double-blink pattern)**
6. Connect to MQTT — 3 attempts, 5s each **(LED: blink + 1s hold)**; on failure: error LED 60s → LED off → deep sleep
7. Always publish status `"OK"` or `"NOK"` to `…/status`
8. If read successful, publish `temperature` value to `…/temperature`
9. If read successful, publish `humidity` value to `…/humidity`
10. Call `client.loop()` and wait 100ms to flush send buffer
11. Explicitly disconnect from MQTT broker
12. Turn LED off (`digitalWrite(LED_BUILTIN, HIGH)`) — LED_BUILTIN is active LOW; leaving it on draws current during sleep
13. Enter deep sleep for `sleep.normal_s`

### Deep Sleep

- In Iteration 1: always sleep for `sleep.normal_s` seconds
- On wake, ESP8266 performs a full reset — execution restarts from `setup()`
- Battery-based sleep duration selection is added in Iteration 2
- **Hardware limit**: ESP8266 maximum deep sleep is ~4294s (~71 min); values above this require chained sleep cycles in firmware
- Estimated active time per cycle: ~10–20s typical (WiFi 5–15s + sensor 3s + MQTT 1–3s); maximum ~36s if all WiFi retries are exhausted

---

## Iteration 2 — Battery Monitoring `[PLANNED]`

### Hardware

- Wemos D1 Mini Battery Shield (TP5410-based): LiPo charger + DC-DC boost converter (3.7 V → 5 V)
- Battery voltage is read from **A0** via the shield's built-in resistor divider — this measures
  the raw LiPo voltage (before the boost converter), not the 5 V regulated output
- Scale factor: `battery_v = analogRead(A0) * (4.2f / 1023.0f)` for shield v1.1.0
  (full-scale = 4.2 V, fully charged LiPo; adjust constant if using a different divider ratio)
- The scale factor is a compile-time constant `BATTERY_ADC_SCALE` in `src/main.cpp`

### Battery Voltage

- Read `analogRead(A0)` once per cycle immediately after sensor read (Step 5b)
- Publish to `{topic_root}/esp-{chip_id}/telemetry/voltage` — 2 decimal places string, retain true, QoS 0
- Example: `"3.70"`

### MQTT Topics (change from Iteration 1)

All measurements are now published under a `telemetry/` sub-level:

- `{topic_root}/esp-{chip_id}/telemetry/temperature`
- `{topic_root}/esp-{chip_id}/telemetry/humidity`
- `{topic_root}/esp-{chip_id}/telemetry/voltage`

The status topic is unchanged: `{topic_root}/esp-{chip_id}/status`.

Topic construction uses a new `build_telemetry_topic()` helper in `src/utils.h`:
`snprintf(buf, len, "%s/%s/telemetry/%s", root, device, sub)`

### Status Payload (change from Iteration 1)

Two new status values are added. Priority (top-to-bottom, first match wins):

| Condition | Status |
| --- | --- |
| `battery_v <= battery.critical_v` | `"BAT_CRIT"` |
| `battery_v <= battery.low_v` | `"BAT_LOW"` |
| sensor read succeeded | `"OK"` |
| sensor read failed | `"NOK"` |
| broker-triggered LWT | `"OFFLINE"` |

Battery state takes priority over sensor state — if battery is critical or low, that is the most actionable signal regardless of whether the sensor read succeeded.

### Battery Conservation

Select sleep duration based on measured voltage — evaluate top-to-bottom, first match wins:

| Condition | Sleep duration |
| --- | --- |
| `battery_v <= battery.critical_v` | `sleep.critical_battery_s` |
| `battery_v <= battery.low_v` | `sleep.low_battery_s` |
| `battery_v > battery.low_v` | `sleep.normal_s` |

### Chained Sleep

`ESP.deepSleep()` maximum is ~4294 s (~71 min). `sleep.critical_battery_s` default is 86400 s (24 h) which exceeds this limit. Iteration 2 implements chained sleep:

- Before sleeping, write remaining duration to RTC user memory (8 bytes: magic word + remaining_s)
- Sleep in segments of at most 4294 s
- On wake, check RTC memory; if remaining > 0, sleep another segment without running the full publish cycle — battery state will be rechecked on the next full wake regardless
- RTC user memory slots 64–65 (8 bytes) are reserved for chained sleep state; cleared when the full publish cycle runs

### Publish Flow Changes (from Iteration 1)

- **Step 5b** (new, between Steps 5 and 6): Read `analogRead(A0)`, convert to `battery_v`, print to Serial
- **Step 7** (change): Determine status using priority table above
- **Steps 8 & 9** (change): Topics now use `build_telemetry_topic()` for temperature and humidity
- **Step 9b** (new, after Step 9): Publish `battery_v` formatted as `"X.XX"` to `…/telemetry/voltage`
- **Step 13** (change): Select sleep duration based on `battery_v`; use `sleep_chained()` helper for durations > 4294 s

---

## Iteration 3 — [IDEA]

> Placeholder — describe the feature and goals here when scoped.

### Ideas / Candidates

- **Timestamp in telemetry**: add NTP-sourced timestamp to measurements — enables time-series databases (InfluxDB, Grafana) without relying on broker receive time

### Open Questions

-

### Notes

-
