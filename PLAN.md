# EnvironmentalSensorV3 — Implementation Plan

## Context

ESP8266 (Wemos D1 Mini clone) firmware for an environmental sensor. The project has complete
functional requirements (`REQUIREMENTS.md`) and implementation guidance (`CLAUDE.md`). This plan
describes the multi-agent build pipeline used to produce all source code.

Architecture: **modular PlatformIO `lib/` libraries** — each module is an independent, reusable
library portable to future IoT projects. `src/main.cpp` is a thin orchestrator only.

---

## File Structure

```
lib/
  ConfigManager/
    ConfigManager.h       Config struct, load/save/defaults declarations
    ConfigManager.cpp     LittleFS mount, ArduinoJson parsing, defaults
  WifiPortalManager/
    WifiPortalManager.h   WiFi retry + portal lifecycle declarations
    WifiPortalManager.cpp WiFi.begin() retry, non-blocking portal, process() loop
  MqttClient/
    MqttClient.h          MQTT connect + publish + flush/disconnect declarations
    MqttClient.cpp        client.connect() with LWT, publish, loop()+delay, disconnect
  LedIndicator/
    LedIndicator.h        LED pattern declarations (millis()-based, non-blocking)
    LedIndicator.cpp      All 5 patterns; error pattern uses delay() for watchdog
  DhtSensor/
    DhtSensor.h           DHT read + averaging declarations
    DhtSensor.cpp         3 reads x 1s, discard NaN, return average + bool
src/
  utils.h                 Inline pure-logic functions (NATIVE_TEST-portable)
  main.cpp                Thin orchestrator — 13-step publish flow
test/
  test_all.cpp            Combined Unity tests (flat — PlatformIO v6 only discovers test/ root)
platformio.ini            [env:d1_mini] (default) + [env:native] for host-side tests
PLAN.md                   This file
REVIEW.md                 Requirements compliance checklist (Agent 8)
STATUS.md                 Agent completion log (each agent appends)
TASKS.md                  Live task tracker — check this to see build progress
```

---

## Module Interfaces (locked — do not modify after Agent 1)

Naming: library folders/headers use PascalCase; functions/variables use `snake_case`; constants use `UPPER_CASE`.

### ConfigManager.h
```cpp
struct Config {
    bool wifi_reset;
    char mqtt_server[64]; int mqtt_port; char mqtt_topic_root[64];
    char mqtt_username[64]; char mqtt_password[64];
    int sleep_normal_s; int sleep_low_battery_s; int sleep_critical_battery_s;
    float battery_low_v; float battery_critical_v;
};
bool config_load(Config& cfg);           // false = portal must open
void config_save(const Config& cfg);     // writes complete /config.json
void config_apply_defaults(Config& cfg); // sets all fields to hardcoded defaults
```

### WifiPortalManager.h
```cpp
enum WifiResult { WIFI_OK, WIFI_FAILED, PORTAL_SAVED, PORTAL_TIMEOUT };
bool wifi_has_credentials();             // WiFi.SSID().length() > 0
WifiResult wifi_connect(int max_attempts, int attempt_timeout_s, int delay_between_s);
WifiResult wifi_open_portal(WiFiManager& mgr, const char* ap_name, int timeout_s,
                             void (*led_tick)(uint32_t));
```

### MqttClient.h
```cpp
bool mqtt_connect(PubSubClient& client,
                  const char* server, int port, const char* client_id,
                  const char* username, const char* password,
                  const char* lwt_topic, int max_attempts, int attempt_timeout_s);
bool mqtt_publish_status(PubSubClient& client, const char* topic, const char* payload);
bool mqtt_publish_measurement(PubSubClient& client, const char* topic, const char* payload);
void mqtt_flush_and_disconnect(PubSubClient& client);
```

### LedIndicator.h
```cpp
void led_init();
void led_off();
void led_update_wifi(uint32_t now);     // 500ms on/off
void led_update_sensor(uint32_t now);   // 500ms on / 500ms off / 500ms on / 1000ms off
void led_update_mqtt(uint32_t now);     // 500ms on / 500ms off / 1000ms on
void led_update_portal(uint32_t now);   // 1000ms on/off
void led_error_blocking(int duration_ms); // 100ms on/off; uses delay() for watchdog
```

### DhtSensor.h
```cpp
bool dht_read_average(DHT& dht, int num_reads, float& temp_out, float& hum_out);
```

### src/utils.h (inline, NATIVE_TEST-portable)
```cpp
void format_device_name(uint32_t chip_id, char* buf, size_t len); // "esp-a1b2c3"
void build_topic(const char* root, const char* device, const char* sub, char* buf, size_t len);
void build_telemetry_topic(const char* root, const char* device, const char* sub, char* buf, size_t len); // Iteration 2
void format_float_1dp(float val, char* buf, size_t len);           // "22.5"
const char* battery_status_str(float battery_v, float low_v, float critical_v); // "BAT_CRIT" / "BAT_LOW" / nullptr — Iteration 2
```

---

## Agent Pipeline

### Iteration 1 (complete ✅)

```text
Agent 1  — Interfaces + ConfigManager.cpp + platformio.ini + TASKS.md
  |
  +-- Agent 2  — WifiPortalManager.cpp         (parallel)
  +-- Agent 3  — MqttClient.cpp + DhtSensor.cpp (parallel)
  +-- Agent 4  — LedIndicator.cpp               (parallel)
  |
Agent 5  — src/main.cpp (13-step orchestrator)
  |
Agent 6  — test/test_all.cpp (Unity unit tests, flat — PlatformIO v6 constraint)
  |
Agent 7  — Full build verification (pio run -e d1_mini clean)
  |
Agent 8  — REVIEW.md compliance checklist
```

### Iteration 2 (in progress)

```text
Agent 9  — REQUIREMENTS.md rewrite ✅ done by session
Agent 10 — src/utils.h + test/test_all.cpp
           (build_telemetry_topic, battery_status_str, 6 new Unity tests)
Agent 11 — src/main.cpp update
           (ADC read, telemetry topics, voltage publish, battery sleep, chained sleep, status logic)
Agent 12 — Build verification (pio run) + REVIEW.md Iteration 2 section
```

Each agent works in an isolated **git worktree** (its own branch). The orchestrator verifies and
merges to `main` only after the build/tests pass. This produces a `git log --graph` with one
merge commit per agent, making each agent's contribution independently reviewable.

### Orchestrator model — one invocation per agent

The orchestrator is a **general-purpose agent** launched once per child agent (not once for the
entire pipeline). Each invocation handles exactly one child agent end-to-end, then returns
a PASS/HALT report to the session. The session reviews and decides whether to launch the next.

```text
Session → orchestrator-10 → PASS/HALT report → Session review
Session → orchestrator-11 → PASS/HALT report → Session review
Session → orchestrator-12 → PASS/HALT report → Session review
```

### Worktree workflow (per orchestrator invocation)

```text
Orchestrator agent:
  1. git worktree add .worktrees/agentN -b agent/N-description
  2. Update TASKS.md → 🔄 In Progress
  3. Launch child agent with prompt referencing the worktree path
Child agent:
  4. Write files inside the worktree directory
  5. Write logs/agentN-description.md (operations log)
  6. git -C <worktree> add + commit (code + log file)
Orchestrator agent:
  7. Read child agent log; run gate conditions independently:
       a. pio run -e d1_mini  (must exit 0)
       b. pio test -e native  (Agents 10 and 12 only; must exit 0)
       c. Verify expected output files exist and are non-empty
       d. Check no TODO/FIXME markers in changed files
  8. If ALL gates PASS:
       git merge --no-ff agent/N-description -m "Merge agent/N-description: ..."
       git worktree remove .worktrees/agentN
       git branch -d agent/N-description
       Update TASKS.md → ✅ Done; append to STATUS.md
       Return PASS report to session
  9. If any gate FAILS (up to 3 retries for build/test errors, inject full error in retry prompt):
       If repeated identical error OR merge conflict: halt immediately
       On halt: preserve worktree + branch; output structured HALT report to session
```

### File ownership

| Owner | Files |
| --- | --- |
| Session | CLAUDE.md, PLAN.md, REQUIREMENTS.md |
| Orchestrator agent | TASKS.md, STATUS.md, REVIEW.md |
| Each child agent | `logs/agentN-description.md` (operations log) |

### Agent log files

Each child agent writes `logs/agentN-description.md` to its worktree before committing. The log
is committed with the code and merged to `main` — creating a permanent audit trail in git history.

Required sections: Files written/modified · Steps performed · Build/test results · Issues encountered · Exit status (PASS/FAIL)

### Halt and report protocol

On failure the orchestrator outputs:

```text
HALT — Agent N failed
Failure type: <build | test | merge-conflict | file-missing | repeated-error>
Attempts: N/3
Last error: <full stderr / test output>
Git state: branch agent/N-xxx at <hash>, worktree .worktrees/agentN PRESERVED
Recommended action: <inspect | re-brief with corrected spec | manual fix>
```

Worktree and branch are NOT cleaned up on halt — session must inspect and clean up after review.

---

## Agent 5 — main.cpp Self-Check

Before declaring done, Agent 5 must verify:
- `(uint64_t)sleep_s * 1000000ULL` — not `sleep_s * 1000000` (int overflow > 71 min)
- `config_save()` with `wifi.reset = false` called **before** `wifi_open_portal()` (scenario 3)
- `setSaveConfigCallback()` registered before every portal open
- Scenario 1 (no credentials): `autoConnect()` with 600s timeout
- Scenarios 2 & 3: `startConfigPortal()` with 300s timeout
- Normal WiFi: `wifi_connect()` using `WiFi.begin()` — never `autoConnect()`
- `led_off()` before every `ESP.deepSleep()` including both error paths
- `mqtt_flush_and_disconnect()` before `led_off()` + `deepSleep()`
- `format_device_name(ESP.getChipId(), device_name, sizeof(device_name))`
- Serial warning if `sleep_normal_s > 4294`

---

## Agent 11 — main.cpp Self-Check (Iteration 2)

Before declaring done, Agent 11 must verify:

- `#define BATTERY_ADC_SCALE (4.2f / 1023.0f)` defined near top of file
- `analogRead(A0) * BATTERY_ADC_SCALE` — uses A0, not any other pin
- `build_telemetry_topic()` used for temperature, humidity, and voltage topics
- `build_topic()` still used for status topic (no telemetry prefix)
- `battery_status_str()` used to determine status; battery priority applied before sensor OK/NOK
- voltage published with `format_float_1dp()` + `mqtt_publish_measurement()` to `topic_volt`
- Sleep duration selected from `battery_v` vs thresholds (critical → low → normal)
- `sleep_chained()` helper defined; never calls `ESP.deepSleep()` directly in Step 13
- `SLEEP_MAGIC` sentinel written to RTC slots 64–65; cleared on full publish cycle
- `(uint64_t)chunk * 1000000ULL` — no int overflow in deepSleep call
- `led_off()` called inside `sleep_chained()` before `ESP.deepSleep()`

---

## Multi-Session Management

### Tracking Progress
Open `TASKS.md` to see the current build status. Each row reflects an agent's state:
`⬜ Pending` / `🔄 In Progress` / `✅ Done` / `❌ Failed`

### Safe Interrupt Points
Between any two agents (or before the Agents 2–4 parallel batch). Each agent is atomic —
it completes and self-verifies before control returns.

**To stop**: don't approve the next agent invocation. All prior work is committed.

**To resume**: tell Claude — "Continue the EnvironmentalSensorV3 build — check TASKS.md and STATUS.md"

### Plan Usage Limits
If a session hits its limit: in-flight agents complete, work is committed, TASKS.md is updated.
Start a new session with the resume instruction above — no manual state restoration needed.

### Git Checkpoints
Committed after each agent batch with messages like:

```text
feat: Agent 1 — interfaces + ConfigManager + platformio.ini native env
feat: Agents 2-4 — WifiPortalManager, MqttClient, DhtSensor, LedIndicator
```

---

## Verification

| What | Method | Agent |
| --- | --- | --- |
| Each lib compiles | `pio run -e d1_mini` incremental | 1–5 |
| Pure logic correct | `pio test -e native` all green (8 tests) | 6 |
| Full integrated build | `pio run -e d1_mini` clean | 7 |
| Iteration 1 requirements | REVIEW.md (file:line mapping) | 8 |
| Iteration 2 pure logic | `pio test -e native` all green (14 tests) | 10 |
| Iteration 2 full build | `pio run` clean | 12 |
| Iteration 2 requirements | REVIEW.md Iteration 2 section | 12 |
| WiFi / MQTT / sensor / LED / sleep / battery | Manual hardware test | Human |
