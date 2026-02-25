#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <DHT.h>
#include "ConfigManager.h"
#include "WifiPortalManager.h"
#include "MqttClient.h"
#include "LedIndicator.h"
#include "DhtSensor.h"
#include "utils.h"

#define DHT_PIN           14    // D5 = GPIO14
#define DHT_TYPE          DHT11
#define NUM_READS         3
#define BATTERY_ADC_SCALE (4.2f / 1023.0f)  // Wemos D1 Mini Battery Shield v1.1.0
#define SLEEP_MAGIC       0xDEADBEEF
#define SLEEP_MAX_S       4294

DHT dht(DHT_PIN, DHT_TYPE);
PubSubClient mqtt_client;

// -- Helper: chained sleep for durations > SLEEP_MAX_S ───────────────────────
// Persists remaining duration in RTC user memory (offset 0, 8 bytes).
// Sleeps in at most SLEEP_MAX_S-second segments. Calls led_off() before sleep.
static void sleep_chained(int total_s) {
    uint32_t rtc[2];
    uint32_t chunk     = ((uint32_t)total_s > SLEEP_MAX_S) ? SLEEP_MAX_S : (uint32_t)total_s;
    uint32_t remaining = (uint32_t)total_s - chunk;
    rtc[0] = (remaining > 0) ? SLEEP_MAGIC : 0;
    rtc[1] = remaining;
    ESP.rtcUserMemoryWrite(0, rtc, sizeof(rtc));
    Serial.printf("[Sleep] Sleeping %us (%us remaining after)\n", chunk, remaining);
    led_off();
    ESP.deepSleep((uint64_t)chunk * 1000000ULL);
}

// -- Helper: register all portal parameters, open portal, loop until closed ──
// Never returns — always calls ESP.restart() or ESP.deepSleep().
// timeout_s=600 for scenario 1 (no creds), 300 for scenarios 2 & 3.
// use_auto_connect=true  -> wm.autoConnect() (scenario 1)
// use_auto_connect=false -> wm.startConfigPortal() (scenarios 2 & 3)
static void open_portal_and_reboot(WiFiManager& wm, Config& cfg,
                                   const char* ap_name, int timeout_s,
                                   bool use_auto_connect) {
    char port_buf[8];
    snprintf(port_buf, sizeof(port_buf), "%d", cfg.mqtt_port);
    char sleep_normal_buf[8];
    snprintf(sleep_normal_buf, sizeof(sleep_normal_buf), "%d", cfg.sleep_normal_s);
    char sleep_low_buf[8];
    snprintf(sleep_low_buf, sizeof(sleep_low_buf), "%d", cfg.sleep_low_battery_s);
    char sleep_crit_buf[8];
    snprintf(sleep_crit_buf, sizeof(sleep_crit_buf), "%d", cfg.sleep_critical_battery_s);
    char batt_low_buf[8];
    snprintf(batt_low_buf, sizeof(batt_low_buf), "%.1f", cfg.battery_low_v);
    char batt_crit_buf[8];
    snprintf(batt_crit_buf, sizeof(batt_crit_buf), "%.1f", cfg.battery_critical_v);

    WiFiManagerParameter p_server("server",      "MQTT Server",                cfg.mqtt_server,      64);
    WiFiManagerParameter p_port("port",          "MQTT Port",                  port_buf,             8);
    WiFiManagerParameter p_user("username",      "MQTT Username",              cfg.mqtt_username,    64);
    WiFiManagerParameter p_pass("password",      "MQTT Password",              cfg.mqtt_password,    64, "type=\"password\"");
    WiFiManagerParameter p_topic("topic_root",   "MQTT Topic Root",            cfg.mqtt_topic_root,  64);
    WiFiManagerParameter p_snorm("sleep_normal", "Sleep Normal (s)",           sleep_normal_buf,     8);
    WiFiManagerParameter p_slow("sleep_low",     "Sleep Low Battery (s)",      sleep_low_buf,        8);
    WiFiManagerParameter p_scrit("sleep_crit",   "Sleep Critical Battery (s)", sleep_crit_buf,       8);
    WiFiManagerParameter p_blow("batt_low",      "Battery Low Voltage",        batt_low_buf,         8);
    WiFiManagerParameter p_bcrit("batt_crit",    "Battery Critical Voltage",   batt_crit_buf,        8);

    wm.addParameter(&p_server);
    wm.addParameter(&p_port);
    wm.addParameter(&p_user);
    wm.addParameter(&p_pass);
    wm.addParameter(&p_topic);
    wm.addParameter(&p_snorm);
    wm.addParameter(&p_slow);
    wm.addParameter(&p_scrit);
    wm.addParameter(&p_blow);
    wm.addParameter(&p_bcrit);

    static Config* cfg_ptr = nullptr;
    cfg_ptr = &cfg;
    static WiFiManagerParameter* params[10];
    params[0] = &p_server;
    params[1] = &p_port;
    params[2] = &p_user;
    params[3] = &p_pass;
    params[4] = &p_topic;
    params[5] = &p_snorm;
    params[6] = &p_slow;
    params[7] = &p_scrit;
    params[8] = &p_blow;
    params[9] = &p_bcrit;

    static bool portal_save_fired = false;
    portal_save_fired = false;

    wm.setSaveConfigCallback([]() {
        Serial.println("[Portal] Save callback fired — writing config");
        strlcpy(cfg_ptr->mqtt_server,     params[0]->getValue(), sizeof(cfg_ptr->mqtt_server));
        cfg_ptr->mqtt_port =              atoi(params[1]->getValue());
        strlcpy(cfg_ptr->mqtt_username,   params[2]->getValue(), sizeof(cfg_ptr->mqtt_username));
        strlcpy(cfg_ptr->mqtt_password,   params[3]->getValue(), sizeof(cfg_ptr->mqtt_password));
        strlcpy(cfg_ptr->mqtt_topic_root, params[4]->getValue(), sizeof(cfg_ptr->mqtt_topic_root));
        cfg_ptr->sleep_normal_s           = atoi(params[5]->getValue());
        cfg_ptr->sleep_low_battery_s      = atoi(params[6]->getValue());
        cfg_ptr->sleep_critical_battery_s = atoi(params[7]->getValue());
        cfg_ptr->battery_low_v            = atof(params[8]->getValue());
        cfg_ptr->battery_critical_v       = atof(params[9]->getValue());
        cfg_ptr->wifi_reset = false;
        config_save(*cfg_ptr);
        portal_save_fired = true;
        Serial.println("[Portal] Config saved");
    });

    wm.setConfigPortalBlocking(false);
    wm.setConfigPortalTimeout(timeout_s);

    if (use_auto_connect) {
        wm.autoConnect(ap_name);
    } else {
        wm.startConfigPortal(ap_name);
    }

    Serial.printf("[Portal] AP: %s (timeout %ds)\n", ap_name, timeout_s);

    while (wm.getConfigPortalActive()) {
        wm.process();
        led_update_portal(millis());
        yield();
    }

    led_off();
    if (portal_save_fired) {
        Serial.println("[Portal] Saved — rebooting");
        delay(200);
        ESP.restart();
    } else {
        Serial.println("[Portal] Timed out — sleeping 300s");
        ESP.deepSleep((uint64_t)300 * 1000000ULL);
    }
}

// -- setup: full publish cycle ────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\n[Boot] EnvironmentalSensorV3 starting");
    dht.begin();
    led_init();

    // -- Chained sleep continuation check ────────────────────────────────────
    // If RTC magic is present and remaining > 0, continue sleeping without
    // running the full publish cycle (battery state rechecked on next full wake).
    {
        uint32_t rtc[2];
        ESP.rtcUserMemoryRead(0, rtc, sizeof(rtc));
        if (rtc[0] == SLEEP_MAGIC && rtc[1] > 0) {
            uint32_t remaining = rtc[1];
            uint32_t chunk = (remaining > SLEEP_MAX_S) ? SLEEP_MAX_S : remaining;
            remaining -= chunk;
            rtc[0] = (remaining > 0) ? SLEEP_MAGIC : 0;
            rtc[1] = remaining;
            ESP.rtcUserMemoryWrite(0, rtc, sizeof(rtc));
            Serial.printf("[Sleep] Chained: sleeping %us more (%us remaining after)\n",
                          chunk, remaining);
            led_off();
            ESP.deepSleep((uint64_t)chunk * 1000000ULL);
            return;
        }
        // Full publish cycle — clear chained sleep state
        rtc[0] = 0; rtc[1] = 0;
        ESP.rtcUserMemoryWrite(0, rtc, sizeof(rtc));
    }

    // Device identity
    char device_name[16];
    format_device_name(ESP.getChipId(), device_name, sizeof(device_name));
    Serial.print("[Boot] Device: ");
    Serial.println(device_name);

    char ap_name[24];
    snprintf(ap_name, sizeof(ap_name), "EnvSensor-%06x", ESP.getChipId());

    // -- Step 1: Load config ──────────────────────────────────────────────────
    Config cfg;
    WiFiManager wm;
    bool config_ok = config_load(cfg);

    if (!config_ok) {
        Serial.println("[Config] Load failed — opening portal (scenario 2, 5min timeout)");
        config_apply_defaults(cfg);
        open_portal_and_reboot(wm, cfg, ap_name, 300, false);
    }

    // -- Step 2: wifi.reset handling (scenario 3) ────────────────────────────
    if (cfg.wifi_reset) {
        Serial.println("[Config] wifi.reset=true — writing false, clearing creds, opening portal (scenario 3, 5min)");
        cfg.wifi_reset = false;
        config_save(cfg);
        WiFi.disconnect(true);
        delay(200);
        open_portal_and_reboot(wm, cfg, ap_name, 300, false);
    }

    // -- Step 3: First boot — no saved credentials (scenario 1) ──────────────
    if (!wifi_has_credentials()) {
        Serial.println("[WiFi] No saved credentials — opening portal (scenario 1, 10min timeout)");
        open_portal_and_reboot(wm, cfg, ap_name, 600, true);
    }

    // -- Step 4: Connect to WiFi ──────────────────────────────────────────────
    // WiFi.begin() with no args uses saved credentials from last autoConnect() session.
    // Retry loop inlined here so led_update_wifi() can be called in the polling loop.
    Serial.println("[WiFi] Connecting with saved credentials...");
    {
        bool wifi_connected = false;

        for (int attempt = 1; attempt <= 3 && !wifi_connected; attempt++) {
            Serial.printf("[WiFi] Attempt %d/3...\n", attempt);
            WiFi.begin();
            unsigned long deadline = millis() + 10000UL;
            while (millis() < deadline) {
                led_update_wifi(millis());
                if (WiFi.status() == WL_CONNECTED) {
                    wifi_connected = true;
                    break;
                }
                delay(100);
            }
            if (!wifi_connected) {
                Serial.printf("[WiFi] Attempt %d/3 failed\n", attempt);
                if (attempt < 3) {
                    unsigned long gap_end = millis() + 2000UL;
                    while (millis() < gap_end) {
                        led_update_wifi(millis());
                        delay(100);
                    }
                }
            }
        }

        if (!wifi_connected) {
            Serial.println("[WiFi] All attempts failed — error LED 60s → deep sleep");
            led_error_blocking(60000);
            led_off();
            ESP.deepSleep((uint64_t)cfg.sleep_normal_s * 1000000ULL);
            return;
        }
        Serial.print("[WiFi] Connected, IP: ");
        Serial.println(WiFi.localIP().toString().c_str());
    }

    // -- Step 5: Read sensor ──────────────────────────────────────────────────
    // LED: 0.5s on / 0.5s off / 0.5s on / 1s off (double-blink), repeating.
    Serial.println("[DHT] Reading sensor (3 reads, 1s apart)...");
    float temp     = 0.0f;
    float hum      = 0.0f;
    bool sensor_ok = false;
    {
        float temp_sum = 0.0f;
        float hum_sum  = 0.0f;
        int   valid    = 0;

        for (int i = 0; i < NUM_READS; i++) {
            float t = dht.readTemperature();
            float h = dht.readHumidity();

            if (!isnan(t) && !isnan(h)) {
                temp_sum += t;
                hum_sum  += h;
                valid++;
                Serial.printf("[DHT] Read %d/%d: %.1f C, %.1f%%\n", i + 1, NUM_READS, t, h);
            } else {
                Serial.printf("[DHT] Read %d/%d: failed (NaN)\n", i + 1, NUM_READS);
            }

            if (i < NUM_READS - 1) {
                unsigned long gap_end = millis() + 1000UL;
                while (millis() < gap_end) {
                    led_update_sensor(millis());
                    delay(10);
                }
            }
        }

        if (valid > 0) {
            temp      = temp_sum / (float)valid;
            hum       = hum_sum  / (float)valid;
            sensor_ok = true;
            Serial.printf("[DHT] Average: %.1f C, %.1f%% (%d valid reads)\n", temp, hum, valid);
        } else {
            Serial.println("[DHT] All reads failed");
        }
    }

    // -- Step 5b: Read battery voltage ────────────────────────────────────────
    int   adc_raw   = analogRead(A0);
    float battery_v = (float)adc_raw * BATTERY_ADC_SCALE;
    Serial.printf("[Batt] ADC raw=%d  voltage=%.2fV\n", adc_raw, battery_v);

    // Build topics — status: build_topic; temp/hum/volt: build_telemetry_topic
    char topic_status[96];
    char topic_temp[96];
    char topic_hum[96];
    char topic_volt[96];
    build_topic(cfg.mqtt_topic_root,           device_name, "status",      topic_status, sizeof(topic_status));
    build_telemetry_topic(cfg.mqtt_topic_root, device_name, "temperature", topic_temp,   sizeof(topic_temp));
    build_telemetry_topic(cfg.mqtt_topic_root, device_name, "humidity",    topic_hum,    sizeof(topic_hum));
    build_telemetry_topic(cfg.mqtt_topic_root, device_name, "voltage",     topic_volt,   sizeof(topic_volt));

    // -- Step 6: Connect to MQTT ──────────────────────────────────────────────
    // LED: 0.5s on / 0.5s off / 1s on, repeating.
    Serial.println("[MQTT] Connecting...");
    {
        bool mqtt_ok = false;

        static WiFiClient wifi_client_mqtt;
        mqtt_client.setClient(wifi_client_mqtt);
        mqtt_client.setServer(cfg.mqtt_server, cfg.mqtt_port);
        mqtt_client.setKeepAlive(60);

        for (int attempt = 1; attempt <= 3 && !mqtt_ok; attempt++) {
            Serial.printf("[MQTT] Attempt %d/3...\n", attempt);
            mqtt_client.connect(device_name,
                                cfg.mqtt_username, cfg.mqtt_password,
                                topic_status, 1, true, "OFFLINE");
            unsigned long deadline = millis() + 5000UL;
            while (millis() < deadline) {
                led_update_mqtt(millis());
                if (mqtt_client.connected()) {
                    mqtt_ok = true;
                    break;
                }
                delay(100);
            }
            if (!mqtt_ok) {
                Serial.printf("[MQTT] Attempt %d/3 failed, state=%d\n", attempt, mqtt_client.state());
                if (attempt < 3) {
                    unsigned long gap_end = millis() + 2000UL;
                    while (millis() < gap_end) {
                        led_update_mqtt(millis());
                        delay(100);
                    }
                }
            }
        }

        if (!mqtt_ok) {
            Serial.println("[MQTT] All attempts failed — error LED 60s → deep sleep");
            led_error_blocking(60000);
            led_off();
            ESP.deepSleep((uint64_t)cfg.sleep_normal_s * 1000000ULL);
            return;
        }
        Serial.println("[MQTT] Connected");
    }

    // -- Step 7: Publish status (battery priority > sensor state) ─────────────
    const char* batt_str   = battery_status_str(battery_v, cfg.battery_low_v, cfg.battery_critical_v);
    const char* status_str = batt_str ? batt_str : (sensor_ok ? "OK" : "NOK");
    mqtt_publish_status(mqtt_client, topic_status, status_str);
    Serial.printf("[MQTT] Published status: %s -> %s\n", topic_status, status_str);

    // -- Steps 8 & 9: Publish temperature and humidity (if sensor read succeeded)
    if (sensor_ok) {
        char val_buf[16];

        format_float_0dp(temp, val_buf, sizeof(val_buf));
        mqtt_publish_measurement(mqtt_client, topic_temp, val_buf);
        Serial.printf("[MQTT] Published temperature: %s -> %s\n", topic_temp, val_buf);

        format_float_0dp(hum, val_buf, sizeof(val_buf));
        mqtt_publish_measurement(mqtt_client, topic_hum, val_buf);
        Serial.printf("[MQTT] Published humidity: %s -> %s\n", topic_hum, val_buf);
    }

    // -- Step 9b: Publish battery voltage (always published) ──────────────────
    {
        char volt_buf[16];
        format_float_1dp(battery_v, volt_buf, sizeof(volt_buf));
        mqtt_publish_measurement(mqtt_client, topic_volt, volt_buf);
        Serial.printf("[MQTT] Published voltage: %s -> %s\n", topic_volt, volt_buf);
    }

    // -- Steps 10 & 11: Flush send buffer and disconnect ──────────────────────
    mqtt_flush_and_disconnect(mqtt_client);
    Serial.println("[MQTT] Disconnected");

    // -- Step 13: Battery-based sleep (led_off called inside sleep_chained) ───
    int sleep_s;
    if (battery_v <= cfg.battery_critical_v)
        sleep_s = cfg.sleep_critical_battery_s;
    else if (battery_v <= cfg.battery_low_v)
        sleep_s = cfg.sleep_low_battery_s;
    else
        sleep_s = cfg.sleep_normal_s;

    Serial.printf("[Sleep] battery=%.2fV -> sleep %ds\n", battery_v, sleep_s);
    sleep_chained(sleep_s);
}

void loop() {
    // Never reached — ESP8266 restarts from setup() after each deep sleep wake
}
