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

#define DHT_PIN    14    // D5 = GPIO14
#define DHT_TYPE   DHT11
#define NUM_READS  3

DHT dht(DHT_PIN, DHT_TYPE);
PubSubClient mqtt_client;

// ── Helper: register all portal parameters, open portal, loop until closed, reboot ─
// Never returns — always calls ESP.restart().
// timeout_s=600 for scenario 1 (no creds), 300 for scenarios 2 & 3.
// use_auto_connect=true  → wm.autoConnect() (scenario 1)
// use_auto_connect=false → wm.startConfigPortal() (scenarios 2 & 3)
static void open_portal_and_reboot(WiFiManager& wm, Config& cfg,
                                   const char* ap_name, int timeout_s,
                                   bool use_auto_connect) {
    // Build string defaults for numeric / float parameters
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

    WiFiManagerParameter p_server("server",      "MQTT Server",               cfg.mqtt_server,      64);
    WiFiManagerParameter p_port("port",          "MQTT Port",                 port_buf,             8);
    WiFiManagerParameter p_user("username",      "MQTT Username",             cfg.mqtt_username,    64);
    WiFiManagerParameter p_pass("password",      "MQTT Password",             cfg.mqtt_password,    64, "type=\"password\"");
    WiFiManagerParameter p_topic("topic_root",   "MQTT Topic Root",           cfg.mqtt_topic_root,  64);
    WiFiManagerParameter p_snorm("sleep_normal", "Sleep Normal (s)",          sleep_normal_buf,     8);
    WiFiManagerParameter p_slow("sleep_low",     "Sleep Low Battery (s)",     sleep_low_buf,        8);
    WiFiManagerParameter p_scrit("sleep_crit",   "Sleep Critical Battery (s)", sleep_crit_buf,      8);
    WiFiManagerParameter p_blow("batt_low",      "Battery Low Voltage",       batt_low_buf,         8);
    WiFiManagerParameter p_bcrit("batt_crit",    "Battery Critical Voltage",  batt_crit_buf,        8);

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

    // Capture cfg by pointer for the save callback (lambda captures not available in all
    // Arduino toolchains, so use static pointer — safe here because setup() never returns
    // before the callback fires or the portal times out)
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

    // Portal closed — saved: reboot immediately; timed out: sleep 300s then hardware-reboot on wake
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

// ── setup: full 13-step publish cycle ───────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\n[Boot] EnvironmentalSensorV3 starting");
    dht.begin();
    led_init();

    // Device identity
    char device_name[16];
    format_device_name(ESP.getChipId(), device_name, sizeof(device_name));
    Serial.print("[Boot] Device: ");
    Serial.println(device_name);

    char ap_name[24];
    snprintf(ap_name, sizeof(ap_name), "EnvSensor-%06x", ESP.getChipId());

    // ── Step 1: Load config ──────────────────────────────────────────────────
    Config cfg;
    WiFiManager wm;
    bool config_ok = config_load(cfg);

    if (!config_ok) {
        // Scenario 2: LittleFS mount fail / file not found / invalid JSON
        Serial.println("[Config] Load failed — opening portal (scenario 2, 5min timeout)");
        config_apply_defaults(cfg);
        open_portal_and_reboot(wm, cfg, ap_name, 300, false);
        // open_portal_and_reboot never returns
    }

    // ── Step 2: wifi.reset handling (scenario 3) ────────────────────────────
    if (cfg.wifi_reset) {
        Serial.println("[Config] wifi.reset=true — writing false, clearing creds, opening portal (scenario 3, 5min)");
        cfg.wifi_reset = false;
        config_save(cfg);       // Write wifi.reset=false BEFORE opening portal or clearing creds
        WiFi.disconnect(true);  // Erase saved WiFi credentials from flash
        delay(200);
        open_portal_and_reboot(wm, cfg, ap_name, 300, false);
        // open_portal_and_reboot never returns
    }

    // ── Step 3: First boot — no saved credentials (scenario 1) ──────────────
    if (!wifi_has_credentials()) {
        Serial.println("[WiFi] No saved credentials — opening portal (scenario 1, 10min timeout)");
        open_portal_and_reboot(wm, cfg, ap_name, 600, true);
        // open_portal_and_reboot never returns
    }

    // ── Step 4: Connect to WiFi (normal operation — saved credentials exist) ─
    // LED: 0.5s on / 0.5s off, repeating.
    // WiFi.begin() with no args uses saved credentials from last autoConnect() session.
    // Retry loop inlined here so led_update_wifi() can be called in the polling loop.
    Serial.println("[WiFi] Connecting with saved credentials...");
    {
        bool wifi_connected = false;

        for (int attempt = 1; attempt <= 3 && !wifi_connected; attempt++) {
            Serial.printf("[WiFi] Attempt %d/3...\n", attempt);
            WiFi.begin();  // no args — saved credentials
            unsigned long deadline = millis() + 10000UL;  // 10s timeout per attempt
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
                    // 2s fixed delay between attempts — drive LED during the gap
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

    // ── Step 5: Read sensor ──────────────────────────────────────────────────
    // LED: 0.5s on / 0.5s off / 0.5s on / 1s off (double-blink), repeating.
    // Reads inlined here so led_update_sensor() can be called in the inter-read delay.
    Serial.println("[DHT] Reading sensor (3 reads, 1s apart)...");
    float temp = 0.0f;
    float hum  = 0.0f;
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
                // 1s delay between reads — drive LED during this gap
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

    // Build topics (used for MQTT connect LWT and all publishes)
    char topic_status[96];
    char topic_temp[96];
    char topic_hum[96];
    build_topic(cfg.mqtt_topic_root, device_name, "status",      topic_status, sizeof(topic_status));
    build_topic(cfg.mqtt_topic_root, device_name, "temperature", topic_temp,   sizeof(topic_temp));
    build_topic(cfg.mqtt_topic_root, device_name, "humidity",    topic_hum,    sizeof(topic_hum));

    // ── Step 6: Connect to MQTT ──────────────────────────────────────────────
    // LED: 0.5s on / 0.5s off / 1s on, repeating.
    // Retry loop inlined here so led_update_mqtt() can be called in the polling loop.
    Serial.println("[MQTT] Connecting...");
    {
        bool mqtt_ok = false;

        // mqtt_connect() owns its own static WiFiClient — call it with LED-unaware blocking,
        // but inline our own retry loop so we can drive the LED between poll cycles.
        // We bypass mqtt_connect() here and inline the equivalent logic with LED integration.
        static WiFiClient wifi_client_mqtt;
        mqtt_client.setClient(wifi_client_mqtt);
        mqtt_client.setServer(cfg.mqtt_server, cfg.mqtt_port);
        mqtt_client.setKeepAlive(60);

        for (int attempt = 1; attempt <= 3 && !mqtt_ok; attempt++) {
            Serial.printf("[MQTT] Attempt %d/3...\n", attempt);
            mqtt_client.connect(device_name,
                                cfg.mqtt_username, cfg.mqtt_password,
                                topic_status, 1, true, "OFFLINE");
            unsigned long deadline = millis() + 5000UL;  // 5s timeout per attempt
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
                    // 2s fixed delay between attempts — drive LED during the gap
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

    // ── Step 7: Publish status (OK or NOK) ──────────────────────────────────
    const char* status_str = sensor_ok ? "OK" : "NOK";
    mqtt_publish_status(mqtt_client, topic_status, status_str);
    Serial.printf("[MQTT] Published status: %s → %s\n", topic_status, status_str);

    // ── Steps 8 & 9: Publish measurements (only if sensor read succeeded) ────
    if (sensor_ok) {
        char val_buf[16];

        format_float_1dp(temp, val_buf, sizeof(val_buf));
        mqtt_publish_measurement(mqtt_client, topic_temp, val_buf);
        Serial.printf("[MQTT] Published temperature: %s → %s\n", topic_temp, val_buf);

        format_float_1dp(hum, val_buf, sizeof(val_buf));
        mqtt_publish_measurement(mqtt_client, topic_hum, val_buf);
        Serial.printf("[MQTT] Published humidity: %s → %s\n", topic_hum, val_buf);
    }

    // ── Steps 10 & 11: Flush send buffer and disconnect ──────────────────────
    mqtt_flush_and_disconnect(mqtt_client);
    Serial.println("[MQTT] Disconnected");

    // ── Step 12: LED off (active LOW — HIGH = off) ───────────────────────────
    led_off();

    // ── Step 13: Deep sleep ──────────────────────────────────────────────────
    if (cfg.sleep_normal_s > 4294) {
        Serial.println("[Sleep] WARNING: sleep_normal_s exceeds ESP8266 hardware deep sleep limit (~4294s); device will wake earlier than configured");
    }
    Serial.printf("[Sleep] Entering deep sleep for %ds\n", cfg.sleep_normal_s);
    // Use uint64_t cast to prevent silent integer overflow for values above ~71 minutes
    ESP.deepSleep((uint64_t)cfg.sleep_normal_s * 1000000ULL);
}

void loop() {
    // Never reached — ESP8266 restarts from setup() after each deep sleep wake
}
