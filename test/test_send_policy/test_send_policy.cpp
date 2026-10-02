#include <unity.h>
#include "send_policy.h"

void setUp() {}
void tearDown() {}

void test_2xx_is_ok() {
    TEST_ASSERT_EQUAL_INT((int)SendResult::Ok, (int)classifySend(200));
    TEST_ASSERT_EQUAL_INT((int)SendResult::Ok, (int)classifySend(204));
}

void test_read_timeout_counts_as_sent() {
    // The server runs the whole sequence before replying; the request did arrive.
    TEST_ASSERT_EQUAL_INT((int)SendResult::Ok, (int)classifySend(kHttpReadTimeout));
}

void test_non_2xx_is_failed() {
    TEST_ASSERT_EQUAL_INT((int)SendResult::Failed, (int)classifySend(404));
    TEST_ASSERT_EQUAL_INT((int)SendResult::Failed, (int)classifySend(500));
}

void test_connection_errors_are_unreachable() {
    TEST_ASSERT_EQUAL_INT((int)SendResult::Unreachable, (int)classifySend(-1));  // refused
    TEST_ASSERT_EQUAL_INT((int)SendResult::Unreachable, (int)classifySend(-4));  // not connected
}

void test_gesture_stale_after_1000ms() {
    TEST_ASSERT_FALSE(gestureStale(5000, 5000));
    TEST_ASSERT_FALSE(gestureStale(5000, 6000));
    TEST_ASSERT_TRUE(gestureStale(5000, 6001));
}

void test_gesture_stale_across_millis_rollover() {
    TEST_ASSERT_FALSE(gestureStale(0xFFFFFF00u, 0x00000010u));  // 272 ms later
    TEST_ASSERT_TRUE(gestureStale(0xFFFFFF00u, 0x00000400u));   // 1280 ms later
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_2xx_is_ok);
    RUN_TEST(test_read_timeout_counts_as_sent);
    RUN_TEST(test_non_2xx_is_failed);
    RUN_TEST(test_connection_errors_are_unreachable);
    RUN_TEST(test_gesture_stale_after_1000ms);
    RUN_TEST(test_gesture_stale_across_millis_rollover);
    return UNITY_END();
}
