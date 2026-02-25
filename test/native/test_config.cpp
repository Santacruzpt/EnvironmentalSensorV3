#include <unity.h>
#include <string.h>
#include "ConfigManager.h"

void setUp(void) {}
void tearDown(void) {}

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

void test_defaults_overwrite_existing_values(void) {
    // config_apply_defaults unconditionally sets all fields —
    // it is called before JSON parsing so JSON values overwrite it afterward.
    // This test confirms pre-set values ARE overwritten by config_apply_defaults.
    Config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mqtt_port = 1885;
    cfg.sleep_normal_s = 120;

    config_apply_defaults(cfg);

    // After apply_defaults, port is reset to 1883 (unconditional overwrite)
    TEST_ASSERT_EQUAL_INT(1883, cfg.mqtt_port);
    TEST_ASSERT_EQUAL_INT(60, cfg.sleep_normal_s);
    // All other fields also at defaults
    TEST_ASSERT_EQUAL_STRING("devices", cfg.mqtt_topic_root);
    TEST_ASSERT_EQUAL_INT(300, cfg.sleep_low_battery_s);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_defaults_all_fields);
    RUN_TEST(test_defaults_overwrite_existing_values);
    return UNITY_END();
}
