#include <unity.h>
#include "show_gate.h"

void setUp() {}
void tearDown() {}

void test_start_is_allowed_when_idle() {
    TEST_ASSERT_TRUE(gestureAllowed(Gesture::Single, false));
}

void test_everything_else_is_ignored_when_idle() {
    const Gesture blocked[] = {Gesture::Double, Gesture::Triple, Gesture::Quad, Gesture::Many, Gesture::Long};
    for (Gesture g : blocked) TEST_ASSERT_FALSE(gestureAllowed(g, false));
}

void test_everything_is_allowed_while_playing() {
    const Gesture all[] = {Gesture::Single, Gesture::Double, Gesture::Triple, Gesture::Quad, Gesture::Many, Gesture::Long};
    for (Gesture g : all) TEST_ASSERT_TRUE(gestureAllowed(g, true));
}

void test_parse_playing_from_player_state() {
    TEST_ASSERT_TRUE(parseIsPlaying("{\"current_song\":{\"id\":3},\"is_playing\":true,\"volume\":70.0}"));
    TEST_ASSERT_TRUE(parseIsPlaying("{\"is_playing\": true}"));
}

void test_parse_not_playing_from_player_state() {
    TEST_ASSERT_FALSE(parseIsPlaying("{\"current_song\":null,\"queue_length\":5,\"is_playing\":false,\"volume\":70.0}"));
    TEST_ASSERT_FALSE(parseIsPlaying("{\"is_playing\": false}"));
}

void test_parse_garbage_is_not_playing() {
    TEST_ASSERT_FALSE(parseIsPlaying(""));
    TEST_ASSERT_FALSE(parseIsPlaying(nullptr));
    TEST_ASSERT_FALSE(parseIsPlaying("{\"detail\":\"Not Found\"}"));
    TEST_ASSERT_FALSE(parseIsPlaying("{\"is_playing\""));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_start_is_allowed_when_idle);
    RUN_TEST(test_everything_else_is_ignored_when_idle);
    RUN_TEST(test_everything_is_allowed_while_playing);
    RUN_TEST(test_parse_playing_from_player_state);
    RUN_TEST(test_parse_not_playing_from_player_state);
    RUN_TEST(test_parse_garbage_is_not_playing);
    return UNITY_END();
}
