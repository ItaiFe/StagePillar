#include <unity.h>
#include "show_latch.h"

void setUp() {}
void tearDown() {}

void test_follows_reports() {
    ShowLatch s;
    TEST_ASSERT_FALSE(s.running(0));
    s.report(true);
    TEST_ASSERT_TRUE(s.running(10));
    s.report(false);
    TEST_ASSERT_FALSE(s.running(20));
}

void test_start_counts_as_running_until_the_poll_catches_up() {
    ShowLatch s;
    s.assumeStarted(1000);
    s.report(false);  // poll from before the server handled start
    TEST_ASSERT_TRUE(s.running(3999));
    TEST_ASSERT_FALSE(s.running(4000));
}

void test_stop_counts_as_idle_until_the_poll_catches_up() {
    ShowLatch s;
    s.report(true);
    s.assumeStopped(1000);
    s.report(true);  // stale poll: server has not handled stop yet
    TEST_ASSERT_FALSE(s.running(1001));
    TEST_ASSERT_FALSE(s.running(3999));
    TEST_ASSERT_TRUE(s.running(4000));
}

void test_stop_cancels_a_start_assumption() {
    ShowLatch s;
    s.assumeStarted(1000);
    s.assumeStopped(1500);
    TEST_ASSERT_FALSE(s.running(1600));
}

void test_start_cancels_a_stop_assumption() {
    ShowLatch s;
    s.assumeStopped(1000);
    s.assumeStarted(1500);
    TEST_ASSERT_TRUE(s.running(1600));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_follows_reports);
    RUN_TEST(test_start_counts_as_running_until_the_poll_catches_up);
    RUN_TEST(test_stop_counts_as_idle_until_the_poll_catches_up);
    RUN_TEST(test_stop_cancels_a_start_assumption);
    RUN_TEST(test_start_cancels_a_stop_assumption);
    return UNITY_END();
}
