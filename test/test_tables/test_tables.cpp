#include <unity.h>
#include "actions.h"
#include "led_pattern.h"

void setUp() {}
void tearDown() {}

void test_action_mapping() {
    TEST_ASSERT_EQUAL_STRING("start", actionFor(Gesture::Single));
    TEST_ASSERT_EQUAL_STRING("claps", actionFor(Gesture::Double));
    TEST_ASSERT_EQUAL_STRING("special", actionFor(Gesture::Triple));
    TEST_ASSERT_EQUAL_STRING("skip", actionFor(Gesture::Quad));
    TEST_ASSERT_EQUAL_STRING("stop", actionFor(Gesture::Long));
    TEST_ASSERT_NULL(actionFor(Gesture::None));
}

void test_led_ok_is_one_100ms_blink() {
    TEST_ASSERT_TRUE(ledLevel(Flash::Ok, 0, true, 5000));
    TEST_ASSERT_TRUE(ledLevel(Flash::Ok, 99, true, 5000));
    TEST_ASSERT_FALSE(ledLevel(Flash::Ok, 100, true, 5000));
}

void test_led_fail_is_three_fast_blinks() {
    // 60 ms on / 60 ms off, three times, over 360 ms.
    const uint32_t onTimes[] = {0, 59, 120, 179, 240, 299};
    const uint32_t offTimes[] = {60, 119, 180, 239, 300, 359, 360, 1000};
    for (uint32_t ms : onTimes) TEST_ASSERT_TRUE(ledLevel(Flash::Fail, ms, true, 5000));
    for (uint32_t ms : offTimes) TEST_ASSERT_FALSE(ledLevel(Flash::Fail, ms, true, 5000));
}

void test_led_slow_blink_when_wifi_down() {
    TEST_ASSERT_TRUE(ledLevel(Flash::None, 0, false, 0));
    TEST_ASSERT_TRUE(ledLevel(Flash::None, 0, false, 499));
    TEST_ASSERT_FALSE(ledLevel(Flash::None, 0, false, 500));
    TEST_ASSERT_FALSE(ledLevel(Flash::None, 0, false, 999));
    TEST_ASSERT_TRUE(ledLevel(Flash::None, 0, false, 1000));
}

void test_led_off_when_idle_and_connected() {
    TEST_ASSERT_FALSE(ledLevel(Flash::None, 0, true, 0));
    TEST_ASSERT_FALSE(ledLevel(Flash::Ok, 5000, true, 5000));
}

void test_led_flash_overrides_wifi_blink() {
    // At nowMs=500 the slow blink would be off, but a fresh Ok flash shows.
    TEST_ASSERT_TRUE(ledLevel(Flash::Ok, 10, false, 500));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_action_mapping);
    RUN_TEST(test_led_ok_is_one_100ms_blink);
    RUN_TEST(test_led_fail_is_three_fast_blinks);
    RUN_TEST(test_led_slow_blink_when_wifi_down);
    RUN_TEST(test_led_off_when_idle_and_connected);
    RUN_TEST(test_led_flash_overrides_wifi_blink);
    return UNITY_END();
}
