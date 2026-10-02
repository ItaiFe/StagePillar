#include <unity.h>
#include "pillar_player.h"

static const uint16_t N = kPlanPixels;
static Rgb frame[N];
static Rgb expected[N];

static bool sameFrame() {
    for (uint16_t i = 0; i < N; i++) {
        if (frame[i].r != expected[i].r || frame[i].g != expected[i].g || frame[i].b != expected[i].b) return false;
    }
    return true;
}

// Expected frame: a fresh render of the plan started at startedAt (seeded with it).
static bool showingPlan(Plan plan, uint32_t startedAt, uint32_t now) {
    static SequenceRenderer r;
    r.start(plan, startedAt, startedAt);
    r.render(now, expected, N);
    return sameFrame();
}

static bool showing(Slot slot, uint32_t startedAt, uint32_t now) {
    return showingPlan(defaultPlan(slot), startedAt, now);
}

static EffectStep firstStep(Slot slot) {
    Plan p = defaultPlan(slot);
    EffectStep s{};
    readEffectStep(*p.src, *p.index, 0, s);
    return s;
}

// A one-shot preview: solid blue for 300 ms.
static uint8_t previewBuf[64];
static MemorySource* previewSrc;
static PlanIndex previewIndex;
static Plan previewPlan(bool loop) {
    const EffectStep steps[] = {{EffectId::Solid, 300, 255, Direction::Up, 16, {{0, 0, 255}}}};
    uint32_t len = encodeEffectPlan(9, loop, steps, 1, previewBuf, sizeof(previewBuf));
    delete previewSrc;
    previewSrc = new MemorySource(previewBuf, len);
    planIndex(*previewSrc, previewIndex);
    return Plan{previewSrc, &previewIndex};
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

void test_slot_names_match_the_contract() {
    TEST_ASSERT_EQUAL_STRING("idle", slotName(Slot::Idle));
    TEST_ASSERT_EQUAL_STRING("start", slotName(Slot::Start));
    TEST_ASSERT_EQUAL_STRING("claps", slotName(Slot::Claps));
    TEST_ASSERT_EQUAL_STRING("special", slotName(Slot::Special));
    TEST_ASSERT_EQUAL_STRING("skip", slotName(Slot::Skip));
    TEST_ASSERT_EQUAL_STRING("stop", slotName(Slot::Stop));
}

void test_defaults_match_the_contract() {
    TEST_ASSERT_TRUE(defaultPlan(Slot::Idle).index->loop);
    EffectStep idle = firstStep(Slot::Idle);
    TEST_ASSERT_EQUAL_INT((int)EffectId::Rainbow, (int)idle.effect);
    TEST_ASSERT_EQUAL_UINT16(256 * 80, idle.durationMs);

    TEST_ASSERT_TRUE(defaultPlan(Slot::Start).index->loop);
    EffectStep start = firstStep(Slot::Start);
    TEST_ASSERT_EQUAL_INT((int)EffectId::Comet, (int)start.effect);
    TEST_ASSERT_EQUAL_INT((int)Direction::Bounce, (int)start.direction);
    TEST_ASSERT_EQUAL_UINT8(24, start.speedX16);
    TEST_ASSERT_EQUAL_UINT16(2100, start.durationMs);

    TEST_ASSERT_EQUAL_UINT16(1200, firstStep(Slot::Claps).durationMs);
    TEST_ASSERT_EQUAL_UINT16(1500, firstStep(Slot::Special).durationMs);
    TEST_ASSERT_EQUAL_UINT16(500, firstStep(Slot::Skip).durationMs);
    TEST_ASSERT_EQUAL_UINT16(1500, firstStep(Slot::Stop).durationMs);
    const Slot oneShots[] = {Slot::Claps, Slot::Special, Slot::Skip, Slot::Stop};
    for (Slot s : oneShots) TEST_ASSERT_FALSE(defaultPlan(s).index->loop);
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
    p.render(180000, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Start, 1000, 180000));
}

void test_gesture_returns_to_play_loop_while_running() {
    PillarPlayer p;
    p.setShowRunning(true, 0);
    p.trigger(Slot::Claps, 10000);
    p.render(10500, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Claps, 10000, 10500));
    p.render(11200, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Start, 11200, 11200));
}

void test_gesture_returns_to_idle_when_not_running() {
    PillarPlayer p;
    p.trigger(Slot::Skip, 1000);
    p.render(1500, frame, N);
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
    p.render(61500, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Idle, 61500, 61500));
}

void test_stop_gesture_plays_stop_once() {
    PillarPlayer p;
    p.trigger(Slot::Start, 0);
    p.setShowRunning(true, 0);
    p.trigger(Slot::Stop, 5000);
    p.setShowRunning(false, 5800);
    p.render(6000, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Stop, 5000, 6000));
}

void test_preview_interrupts_and_returns_to_base_when_done() {
    PillarPlayer p;
    Plan preview = previewPlan(false);
    p.render(100, frame, N);
    p.playPreview(preview, 1000);
    p.render(1100, frame, N);
    TEST_ASSERT_TRUE(showingPlan(preview, 1000, 1100));
    p.render(1300, frame, N);  // 300 ms preview over
    TEST_ASSERT_TRUE(showing(Slot::Idle, 1300, 1300));
}

void test_looping_preview_runs_until_ended() {
    PillarPlayer p;
    Plan preview = previewPlan(true);
    p.setShowRunning(true, 0);
    p.playPreview(preview, 1000);
    p.render(9000, frame, N);
    TEST_ASSERT_TRUE(showingPlan(preview, 1000, 9000));
    p.endPreview(9500);
    p.render(9600, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Start, 9500, 9600));
}

void test_gesture_ends_a_preview() {
    PillarPlayer p;
    p.playPreview(previewPlan(true), 1000);
    p.trigger(Slot::Stop, 2000);
    p.render(2100, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Stop, 2000, 2100));
    p.endPreview(2200);  // late end must not cut the stop fade short
    p.render(2300, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Stop, 2000, 2300));
}

void test_show_state_change_does_not_cut_a_preview() {
    PillarPlayer p;
    Plan preview = previewPlan(true);
    p.playPreview(preview, 1000);
    p.setShowRunning(true, 1500);
    p.render(1600, frame, N);
    TEST_ASSERT_TRUE(showingPlan(preview, 1000, 1600));
    p.endPreview(2000);
    p.render(2000, frame, N);
    TEST_ASSERT_TRUE(showing(Slot::Start, 2000, 2000));
}

static Plan claps;  // replaced plan for the reload test
static Plan lookupWithCustomClaps(Slot s) { return s == Slot::Claps ? claps : defaultPlan(s); }

void test_reload_restarts_the_active_slot_with_its_new_plan() {
    PillarPlayer p(lookupWithCustomClaps);
    claps = defaultPlan(Slot::Claps);
    p.trigger(Slot::Claps, 1000);
    claps = previewPlan(false);
    p.reload(1100);
    p.render(1200, frame, N);
    TEST_ASSERT_TRUE(showingPlan(claps, 1100, 1200));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_slot_for_each_gesture);
    RUN_TEST(test_slot_names_match_the_contract);
    RUN_TEST(test_defaults_match_the_contract);
    RUN_TEST(test_starts_in_idle);
    RUN_TEST(test_start_plays_the_play_loop_for_the_whole_song);
    RUN_TEST(test_gesture_returns_to_play_loop_while_running);
    RUN_TEST(test_gesture_returns_to_idle_when_not_running);
    RUN_TEST(test_music_started_elsewhere_switches_idle_to_play);
    RUN_TEST(test_running_mid_gesture_lets_the_gesture_finish);
    RUN_TEST(test_song_ending_plays_stop_then_idle);
    RUN_TEST(test_stop_gesture_plays_stop_once);
    RUN_TEST(test_preview_interrupts_and_returns_to_base_when_done);
    RUN_TEST(test_looping_preview_runs_until_ended);
    RUN_TEST(test_gesture_ends_a_preview);
    RUN_TEST(test_show_state_change_does_not_cut_a_preview);
    RUN_TEST(test_reload_restarts_the_active_slot_with_its_new_plan);
    return UNITY_END();
}
