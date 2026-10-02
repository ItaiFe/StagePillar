#include <unity.h>
#include "led_effects.h"

static const uint16_t N = kPillarLeds;
static Rgb frame[N];

static bool isBlack(const Rgb& c) { return c.r == 0 && c.g == 0 && c.b == 0; }

static uint16_t countLit() {
    uint16_t lit = 0;
    for (uint16_t i = 0; i < N; i++) if (!isBlack(frame[i])) lit++;
    return lit;
}

static int firstLit() {
    for (uint16_t i = 0; i < N; i++) if (!isBlack(frame[i])) return i;
    return -1;
}

void setUp() {}
void tearDown() {}

void test_effect_for_each_gesture() {
    TEST_ASSERT_EQUAL_INT((int)Effect::Start, (int)effectFor(Gesture::Single));
    TEST_ASSERT_EQUAL_INT((int)Effect::Claps, (int)effectFor(Gesture::Double));
    TEST_ASSERT_EQUAL_INT((int)Effect::Special, (int)effectFor(Gesture::Triple));
    TEST_ASSERT_EQUAL_INT((int)Effect::Skip, (int)effectFor(Gesture::Quad));
    TEST_ASSERT_EQUAL_INT((int)Effect::Special, (int)effectFor(Gesture::Many));
    TEST_ASSERT_EQUAL_INT((int)Effect::Stop, (int)effectFor(Gesture::Long));
    TEST_ASSERT_EQUAL_INT((int)Effect::None, (int)effectFor(Gesture::None));
}

void test_every_effect_runs_then_ends_within_1500ms() {
    const Effect all[] = {Effect::Start, Effect::Claps, Effect::Special, Effect::Skip, Effect::Stop};
    for (Effect e : all) {
        TEST_ASSERT_TRUE(renderEffect(e, 0, 1, frame, N));
        TEST_ASSERT_TRUE(effectDurationMs(e) <= 1500);
        TEST_ASSERT_TRUE(renderEffect(e, effectDurationMs(e) - 1, 1, frame, N));
        TEST_ASSERT_FALSE(renderEffect(e, effectDurationMs(e), 1, frame, N));
    }
    TEST_ASSERT_FALSE(renderEffect(Effect::None, 0, 1, frame, N));
}

void test_start_sweeps_up_from_the_bottom() {
    renderEffect(Effect::Start, 300, 1, frame, N);
    uint16_t early = countLit();
    TEST_ASSERT_FALSE(isBlack(frame[0]));
    TEST_ASSERT_TRUE(isBlack(frame[N - 1]));
    renderEffect(Effect::Start, 600, 1, frame, N);
    TEST_ASSERT_TRUE(countLit() > early);
}

void test_claps_lights_some_but_not_most_leds() {
    renderEffect(Effect::Claps, 100, 7, frame, N);
    uint16_t lit = countLit();
    TEST_ASSERT_TRUE(lit >= 1);
    TEST_ASSERT_TRUE(lit <= N / 2);
}

void test_special_colours_the_whole_pillar_evenly() {
    renderEffect(Effect::Special, 375, 1, frame, N);
    TEST_ASSERT_FALSE(isBlack(frame[0]));
    for (uint16_t i = 1; i < N; i++) {
        TEST_ASSERT_EQUAL_UINT8(frame[0].r, frame[i].r);
        TEST_ASSERT_EQUAL_UINT8(frame[0].g, frame[i].g);
        TEST_ASSERT_EQUAL_UINT8(frame[0].b, frame[i].b);
    }
}

void test_skip_band_moves_up() {
    renderEffect(Effect::Skip, 100, 1, frame, N);
    int early = firstLit();
    renderEffect(Effect::Skip, 300, 1, frame, N);
    int later = firstLit();
    TEST_ASSERT_TRUE(early >= 0);
    TEST_ASSERT_TRUE(later > early);
}

void test_stop_ends_in_a_dim_red_glow() {
    renderEffect(Effect::Stop, effectDurationMs(Effect::Stop) - 100, 1, frame, N);
    for (uint16_t i = 0; i < N; i++) {
        TEST_ASSERT_TRUE(frame[i].r > 0 && frame[i].r <= 60);
        TEST_ASSERT_EQUAL_UINT8(0, frame[i].g);
        TEST_ASSERT_EQUAL_UINT8(0, frame[i].b);
    }
}

void test_idle_rainbow_flows_up() {
    static Rgb later[N];
    renderIdle(10000, frame, N);
    renderIdle(10000 + kIdleMsPerLed, later, N);
    TEST_ASSERT_FALSE(isBlack(frame[0]));
    for (uint16_t i = 0; i + 1 < N; i++) {
        TEST_ASSERT_EQUAL_UINT8(frame[i].r, later[i + 1].r);
        TEST_ASSERT_EQUAL_UINT8(frame[i].g, later[i + 1].g);
        TEST_ASSERT_EQUAL_UINT8(frame[i].b, later[i + 1].b);
    }
}

void test_fail_overlay_is_two_red_flashes() {
    const uint32_t redAt[] = {0, 99, 200, 299};
    const uint32_t passAt[] = {100, 199};
    for (uint32_t t : redAt) {
        frame[5] = Rgb{1, 2, 3};
        TEST_ASSERT_TRUE(overlayFail(t, frame, N));
        TEST_ASSERT_EQUAL_UINT8(255, frame[5].r);
        TEST_ASSERT_EQUAL_UINT8(0, frame[5].g);
    }
    for (uint32_t t : passAt) {
        frame[5] = Rgb{1, 2, 3};
        TEST_ASSERT_TRUE(overlayFail(t, frame, N));
        TEST_ASSERT_EQUAL_UINT8(1, frame[5].r);
    }
    frame[5] = Rgb{1, 2, 3};
    TEST_ASSERT_FALSE(overlayFail(300, frame, N));
    TEST_ASSERT_EQUAL_UINT8(1, frame[5].r);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_effect_for_each_gesture);
    RUN_TEST(test_every_effect_runs_then_ends_within_1500ms);
    RUN_TEST(test_start_sweeps_up_from_the_bottom);
    RUN_TEST(test_claps_lights_some_but_not_most_leds);
    RUN_TEST(test_special_colours_the_whole_pillar_evenly);
    RUN_TEST(test_skip_band_moves_up);
    RUN_TEST(test_stop_ends_in_a_dim_red_glow);
    RUN_TEST(test_idle_rainbow_flows_up);
    RUN_TEST(test_fail_overlay_is_two_red_flashes);
    return UNITY_END();
}
