#include <unity.h>
#include "pillar_player.h"

static const uint16_t N = 100;
static Rgb frame[N];
static Rgb expected[N];

static bool sameFrame() {
    for (uint16_t i = 0; i < N; i++) {
        if (frame[i].r != expected[i].r || frame[i].g != expected[i].g || frame[i].b != expected[i].b) return false;
    }
    return true;
}

// Expected frame: slot's default sequence at msInto, seeded with the time it started.
static bool showing(Slot slot, uint32_t startedAt, uint32_t now) {
    renderSequence(defaultSequence(slot), now - startedAt, startedAt, expected, N);
    return sameFrame();
}

void setUp() {}
void tearDown() {}

void test_slot_for_each_gesture() {
    TEST_ASSERT_EQUAL_INT((int)Slot::Start, (int)slotFor(Gesture::Single));
    TEST_ASSERT_EQUAL_INT((int)Slot::Claps, (int)slotFor(Gesture::Double));
    TEST_ASSERT_EQUAL_INT((int)Slot::Special, (int)slotFor(Gesture::Triple));
    TEST_ASSERT_EQUAL_INT((int)Slot::Skip, (int)slotFor(Gesture::Quad));
    TEST_ASSERT_EQUAL_INT((int)Slot::Special, (int)slotFor(Gesture::Many));
    TEST_ASSERT_EQUAL_INT((int)Slot::Stop, (int)slotFor(Gesture::Long));
    TEST_ASSERT_EQUAL_INT((int)Slot::None, (int)slotFor(Gesture::None));
}

void test_defaults_match_the_contract() {
    const Sequence& idle = defaultSequence(Slot::Idle);
    TEST_ASSERT_TRUE(idle.loop);
    TEST_ASSERT_EQUAL_INT((int)EffectId::Rainbow, (int)idle.steps[0].effect);
    // The rainbow pattern repeats exactly when the loop restarts: 256 LEDs of shift.
    TEST_ASSERT_EQUAL_UINT16(256 * 80, idle.steps[0].durationMs);

    const Sequence& start = defaultSequence(Slot::Start);
    TEST_ASSERT_TRUE(start.loop);
    TEST_ASSERT_EQUAL_INT((int)EffectId::Comet, (int)start.steps[0].effect);
    TEST_ASSERT_EQUAL_INT((int)Direction::Bounce, (int)start.steps[0].direction);
    TEST_ASSERT_EQUAL_UINT8(24, start.steps[0].speedX16);
    TEST_ASSERT_EQUAL_UINT16(2100, start.steps[0].durationMs);

    const Slot oneShots[] = {Slot::Claps, Slot::Special, Slot::Skip, Slot::Stop};
    for (Slot s : oneShots) TEST_ASSERT_FALSE(defaultSequence(s).loop);
}

void test_looping_sequence_wraps() {
    const Sequence& start = defaultSequence(Slot::Start);
    renderSequence(start, 100, 1, expected, N);
    TEST_ASSERT_TRUE(renderSequence(start, 2100 * 5 + 100, 1, frame, N));
    TEST_ASSERT_TRUE(sameFrame());
}

void test_one_shot_sequence_ends() {
    const Sequence& claps = defaultSequence(Slot::Claps);
    TEST_ASSERT_TRUE(renderSequence(claps, 1199, 1, frame, N));
    TEST_ASSERT_FALSE(renderSequence(claps, 1200, 1, frame, N));
}

void test_starts_in_idle() {
    PillarPlayer p;
    p.render(5000, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Idle, 0, 5000));
}

void test_start_plays_the_play_loop_for_the_whole_song() {
    PillarPlayer p;
    p.trigger(Slot::Start, 1000);
    p.setShowRunning(true, 1500);
    p.render(1500, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Start, 1000, 1500));
    p.render(180000, frame, N);  // three minutes in, still the play loop
    TEST_ASSERT_TRUE(showing(Slot::Start, 1000, 180000));
}

void test_gesture_returns_to_play_loop_while_running() {
    PillarPlayer p;
    p.setShowRunning(true, 0);
    p.trigger(Slot::Claps, 10000);
    p.render(10500, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Claps, 10000, 10500));
    p.render(11200, frame, N);  // claps over at 11200
    TEST_ASSERT_TRUE(showing(Slot::Start, 11200, 11200));
}

void test_gesture_returns_to_idle_when_not_running() {
    PillarPlayer p;
    p.trigger(Slot::Skip, 1000);
    p.render(1500, frame, N);  // skip lasts 500 ms
    TEST_ASSERT_TRUE(showing(Slot::Idle, 1500, 1500));
}

void test_music_started_elsewhere_switches_idle_to_play() {
    PillarPlayer p;
    p.render(1000, frame, N);
    p.setShowRunning(true, 2000);
    p.render(2100, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Start, 2000, 2100));
}

void test_running_mid_gesture_lets_the_gesture_finish() {
    PillarPlayer p;
    p.trigger(Slot::Claps, 1000);
    p.setShowRunning(true, 1100);
    p.render(1500, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Claps, 1000, 1500));
    p.render(2200, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Start, 2200, 2200));
}

void test_song_ending_plays_stop_then_idle() {
    PillarPlayer p;
    p.trigger(Slot::Start, 0);
    p.setShowRunning(true, 0);
    p.setShowRunning(false, 60000);
    p.render(60500, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Stop, 60000, 60500));
    p.render(61500, frame, N);  // stop lasts 1500 ms
    TEST_ASSERT_TRUE(showing(Slot::Idle, 61500, 61500));
}

void test_stop_gesture_plays_stop_once() {
    PillarPlayer p;
    p.trigger(Slot::Start, 0);
    p.setShowRunning(true, 0);
    p.trigger(Slot::Stop, 5000);
    p.setShowRunning(false, 5800);  // the poll catches up; must not restart the fade
    p.render(6000, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Stop, 5000, 6000));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_slot_for_each_gesture);
    RUN_TEST(test_defaults_match_the_contract);
    RUN_TEST(test_looping_sequence_wraps);
    RUN_TEST(test_one_shot_sequence_ends);
    RUN_TEST(test_starts_in_idle);
    RUN_TEST(test_start_plays_the_play_loop_for_the_whole_song);
    RUN_TEST(test_gesture_returns_to_play_loop_while_running);
    RUN_TEST(test_gesture_returns_to_idle_when_not_running);
    RUN_TEST(test_music_started_elsewhere_switches_idle_to_play);
    RUN_TEST(test_running_mid_gesture_lets_the_gesture_finish);
    RUN_TEST(test_song_ending_plays_stop_then_idle);
    RUN_TEST(test_stop_gesture_plays_stop_once);
    return UNITY_END();
}
