// Combined unit test runner for PlatformIO native environment.
// Requires GCC/MinGW in PATH on Windows. Install MinGW-w64 from:
//   https://winlibs.com/  (extract and add bin/ to PATH, then: pio test -e native)
//
// Tests covered:
//   - format_device_name, build_topic, format_float_1dp (utils.h)
//   - config_apply_defaults (ConfigManager.h)

#include <unity.h>
#include <string.h>
#include "utils.h"
#include "ConfigManager.h"

void setUp(void) {}
void tearDown(void) {}

// ── utils: format_device_name ────────────────────────────────────────────────

void test_format_device_name_typical(void) {
    char buf[16];
    format_device_name(0xa1b2c3, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("esp-a1b2c3", buf);
}

void test_format_device_name_zero_padded(void) {
    char buf[16];
    format_device_name(0x000001, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("esp-000001", buf);
}

// ── utils: build_topic ───────────────────────────────────────────────────────

void test_build_topic_standard(void) {
    char buf[96];
    build_topic("devices", "esp-a1b2c3", "temperature", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("devices/esp-a1b2c3/temperature", buf);
}

void test_build_topic_nested_root(void) {
    char buf[96];
    build_topic("home/env", "esp-a1b2c3", "humidity", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("home/env/esp-a1b2c3/humidity", buf);
}

// ── utils: format_float_1dp ──────────────────────────────────────────────────

void test_format_float_1dp_nonzero(void) {
    char buf[16];
    format_float_1dp(22.0f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("22.0", buf);
}

void test_format_float_1dp_zero(void) {
    char buf[16];
    format_float_1dp(0.0f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0.0", buf);
}

// ── config: config_apply_defaults ────────────────────────────────────────────

void test_defaults_all_fields(void) {
    Config cfg;
    memset(&cfg, 0, sizeof(cfg));
    config_apply_defaults(cfg);

    TEST_ASSERT_FALSE(cfg.wifi_reset);
    TEST_ASSERT_EQUAL_STRING("", cfg.mqtt_server);
    TEST_ASSERT_EQUAL_INT(1883, cfg.mqtt_port);
    TEST_ASSERT_EQUAL_STRING("devices", cfg.mqtt_topic_root);
    TEST_ASSERT_EQUAL_STRING("", cfg.mqtt_username);
    TEST_ASSERT_EQUAL_STRING("", cfg.mqtt_password);
    TEST_ASSERT_EQUAL_INT(60, cfg.sleep_normal_s);
    TEST_ASSERT_EQUAL_INT(300, cfg.sleep_low_battery_s);
    TEST_ASSERT_EQUAL_INT(86400, cfg.sleep_critical_battery_s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.5f, cfg.battery_low_v);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.2f, cfg.battery_critical_v);
}

void test_defaults_unconditional_overwrite(void) {
    // config_apply_defaults unconditionally resets ALL fields.
    // config_load calls it first, then overwrites from JSON — so partial JSON
    // configs get defaults for missing keys via this function.
    Config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mqtt_port = 1885;
    cfg.sleep_normal_s = 120;

    config_apply_defaults(cfg);

    TEST_ASSERT_EQUAL_INT(1883, cfg.mqtt_port);        // overwritten
    TEST_ASSERT_EQUAL_INT(60, cfg.sleep_normal_s);     // overwritten
    TEST_ASSERT_EQUAL_STRING("devices", cfg.mqtt_topic_root);
    TEST_ASSERT_EQUAL_INT(300, cfg.sleep_low_battery_s);
}

// ── main ─────────────────────────────────────────────────────────────────────

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_format_device_name_typical);
    RUN_TEST(test_format_device_name_zero_padded);
    RUN_TEST(test_build_topic_standard);
    RUN_TEST(test_build_topic_nested_root);
    RUN_TEST(test_format_float_1dp_nonzero);
    RUN_TEST(test_format_float_1dp_zero);

    RUN_TEST(test_defaults_all_fields);
    RUN_TEST(test_defaults_unconditional_overwrite);

    return UNITY_END();
}
