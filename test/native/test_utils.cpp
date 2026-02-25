#include <unity.h>
#include "utils.h"

void setUp(void) {}
void tearDown(void) {}

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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_format_device_name_typical);
    RUN_TEST(test_format_device_name_zero_padded);
    RUN_TEST(test_build_topic_standard);
    RUN_TEST(test_build_topic_nested_root);
    RUN_TEST(test_format_float_1dp_nonzero);
    RUN_TEST(test_format_float_1dp_zero);
    return UNITY_END();
}
