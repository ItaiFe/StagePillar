#include <string.h>
#include <unity.h>
#include "led_catalogue.h"

static const uint16_t N = 100;
static Rgb frame[N];
static Rgb other[N];

static const Rgb MAGENTA{255, 0, 192};
static const Rgb CYAN{0, 229, 255};
static const Rgb GOLD{255, 176, 0};

static EffectStep step(EffectId effect, uint16_t durationMs, Direction direction = Direction::Up,
                       Rgb c0 = Rgb{0, 0, 0}, Rgb c1 = Rgb{0, 0, 0}, Rgb c2 = Rgb{0, 0, 0},
                       uint8_t speedX16 = 16, uint8_t brightness = 255) {
    return EffectStep{effect, durationMs, brightness, direction, speedX16, {c0, c1, c2}};
}

static bool isBlack(const Rgb& c) { return c.r == 0 && c.g == 0 && c.b == 0; }
static bool same(const Rgb& a, const Rgb& b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

static uint16_t countLit() {
    uint16_t lit = 0;
    for (uint16_t i = 0; i < N; i++) if (!isBlack(frame[i])) lit++;
    return lit;
}

static int brightest() {
    int best = -1, bestSum = 0;
    for (uint16_t i = 0; i < N; i++) {
        int sum = frame[i].r + frame[i].g + frame[i].b;
        if (sum > bestSum) { bestSum = sum; best = i; }
    }
    return best;
}

static int firstLit() {
    for (uint16_t i = 0; i < N; i++) if (!isBlack(frame[i])) return i;
    return -1;
}

void setUp() {}
void tearDown() {}

void test_off_is_black() {
    renderStep(step(EffectId::Off, 1000), 500, 1, frame, N);
    TEST_ASSERT_EQUAL_UINT16(0, countLit());
}

void test_solid_fills_with_first_colour() {
    renderStep(step(EffectId::Solid, 1000, Direction::Up, GOLD), 500, 1, frame, N);
    for (uint16_t i = 0; i < N; i++) TEST_ASSERT_TRUE(same(frame[i], GOLD));
}

void test_brightness_scales_the_step() {
    renderStep(step(EffectId::Solid, 1000, Direction::Up, Rgb{200, 100, 50}, {}, {}, 16, 128), 0, 1, frame, N);
    TEST_ASSERT_EQUAL_UINT8(100, frame[0].r);
    TEST_ASSERT_EQUAL_UINT8(50, frame[0].g);
    TEST_ASSERT_EQUAL_UINT8(25, frame[0].b);
}

void test_rainbow_flows_up_one_led_per_80ms() {
    EffectStep s = step(EffectId::Rainbow, 20480);
    renderStep(s, 1000, 1, frame, N);
    renderStep(s, 1080, 1, other, N);
    TEST_ASSERT_FALSE(isBlack(frame[0]));
    for (uint16_t i = 0; i + 1 < N; i++) TEST_ASSERT_TRUE(same(frame[i], other[i + 1]));
}

void test_rainbow_speed_doubles_the_flow() {
    EffectStep s = step(EffectId::Rainbow, 20480, Direction::Up, {}, {}, {}, 32);
    renderStep(s, 1000, 1, frame, N);
    renderStep(s, 1040, 1, other, N);
    for (uint16_t i = 0; i + 1 < N; i++) TEST_ASSERT_TRUE(same(frame[i], other[i + 1]));
}

void test_rainbow_down_flows_down() {
    EffectStep s = step(EffectId::Rainbow, 20480, Direction::Down);
    renderStep(s, 1000, 1, frame, N);
    renderStep(s, 1080, 1, other, N);
    for (uint16_t i = 0; i + 1 < N; i++) TEST_ASSERT_TRUE(same(frame[i + 1], other[i]));
}

void test_comet_runs_up_in_its_first_colour() {
    // speed 1.0: one pass in 1000 ms.
    EffectStep s = step(EffectId::Comet, 3000, Direction::Up, MAGENTA, CYAN, GOLD);
    renderStep(s, 300, 1, frame, N);
    int early = brightest();
    TEST_ASSERT_TRUE(same(frame[early], MAGENTA));
    renderStep(s, 600, 1, frame, N);
    TEST_ASSERT_TRUE(brightest() > early);
}

void test_comet_uses_its_colours_in_turn() {
    EffectStep s = step(EffectId::Comet, 3000, Direction::Up, MAGENTA, CYAN, GOLD);
    renderStep(s, 1500, 1, frame, N);
    TEST_ASSERT_TRUE(same(frame[brightest()], CYAN));
    renderStep(s, 2500, 1, frame, N);
    TEST_ASSERT_TRUE(same(frame[brightest()], GOLD));
}

void test_comet_bounce_comes_back_down_on_the_second_pass() {
    EffectStep s = step(EffectId::Comet, 3000, Direction::Bounce, MAGENTA, CYAN);
    renderStep(s, 300, 1, frame, N);
    int up1 = brightest();
    renderStep(s, 600, 1, frame, N);
    TEST_ASSERT_TRUE(brightest() > up1);
    renderStep(s, 1300, 1, frame, N);
    int down1 = brightest();
    renderStep(s, 1600, 1, frame, N);
    TEST_ASSERT_TRUE(brightest() < down1);
}

void test_comet_has_a_fading_tail() {
    EffectStep s = step(EffectId::Comet, 3000, Direction::Up, Rgb{200, 0, 0});
    renderStep(s, 500, 1, frame, N);
    int head = brightest();
    TEST_ASSERT_TRUE(head >= 5);
    TEST_ASSERT_TRUE(frame[head - 5].r > 0);
    TEST_ASSERT_TRUE(frame[head - 5].r < frame[head].r);
}

void test_fill_sweeps_up_from_the_bottom() {
    EffectStep s = step(EffectId::Fill, 1500, Direction::Up, Rgb{255, 160, 60});
    renderStep(s, 300, 1, frame, N);
    uint16_t early = countLit();
    TEST_ASSERT_FALSE(isBlack(frame[0]));
    TEST_ASSERT_TRUE(isBlack(frame[N - 1]));
    renderStep(s, 600, 1, frame, N);
    TEST_ASSERT_TRUE(countLit() > early);
}

void test_sparkle_lights_some_but_not_most_leds() {
    renderStep(step(EffectId::Sparkle, 1200, Direction::Up, Rgb{255, 255, 255}), 100, 7, frame, N);
    uint16_t lit = countLit();
    TEST_ASSERT_TRUE(lit >= 1);
    TEST_ASSERT_TRUE(lit <= N / 2);
}

void test_pulse_peaks_in_each_colour_in_turn() {
    EffectStep s = step(EffectId::Pulse, 1500, Direction::Up, Rgb{0, 0, 255}, Rgb{128, 0, 255});
    renderStep(s, 375, 1, frame, N);
    for (uint16_t i = 0; i < N; i++) TEST_ASSERT_TRUE(same(frame[i], (Rgb{0, 0, 255})));
    renderStep(s, 1125, 1, frame, N);
    TEST_ASSERT_TRUE(same(frame[0], (Rgb{128, 0, 255})));
    renderStep(s, 0, 1, frame, N);
    TEST_ASSERT_TRUE(isBlack(frame[0]));
}

void test_band_moves_up() {
    EffectStep s = step(EffectId::Band, 500, Direction::Up, CYAN);
    renderStep(s, 100, 1, frame, N);
    int early = firstLit();
    renderStep(s, 300, 1, frame, N);
    TEST_ASSERT_TRUE(early >= 0);
    TEST_ASSERT_TRUE(firstLit() > early);
}

void test_band_down_moves_down() {
    EffectStep s = step(EffectId::Band, 500, Direction::Down, CYAN);
    renderStep(s, 100, 1, frame, N);
    int early = firstLit();
    renderStep(s, 300, 1, frame, N);
    TEST_ASSERT_TRUE(firstLit() < early);
}

void test_fade_goes_from_first_to_second_colour() {
    EffectStep s = step(EffectId::Fade, 1500, Direction::Up, Rgb{255, 0, 0}, Rgb{40, 0, 0});
    renderStep(s, 0, 1, frame, N);
    TEST_ASSERT_EQUAL_UINT8(255, frame[0].r);
    renderStep(s, 1499, 1, frame, N);
    TEST_ASSERT_TRUE(frame[N - 1].r <= 41);
    TEST_ASSERT_EQUAL_UINT8(0, frame[N - 1].g);
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
    RUN_TEST(test_off_is_black);
    RUN_TEST(test_solid_fills_with_first_colour);
    RUN_TEST(test_brightness_scales_the_step);
    RUN_TEST(test_rainbow_flows_up_one_led_per_80ms);
    RUN_TEST(test_rainbow_speed_doubles_the_flow);
    RUN_TEST(test_rainbow_down_flows_down);
    RUN_TEST(test_comet_runs_up_in_its_first_colour);
    RUN_TEST(test_comet_uses_its_colours_in_turn);
    RUN_TEST(test_comet_bounce_comes_back_down_on_the_second_pass);
    RUN_TEST(test_comet_has_a_fading_tail);
    RUN_TEST(test_fill_sweeps_up_from_the_bottom);
    RUN_TEST(test_sparkle_lights_some_but_not_most_leds);
    RUN_TEST(test_pulse_peaks_in_each_colour_in_turn);
    RUN_TEST(test_band_moves_up);
    RUN_TEST(test_band_down_moves_down);
    RUN_TEST(test_fade_goes_from_first_to_second_colour);
    RUN_TEST(test_fail_overlay_is_two_red_flashes);
    return UNITY_END();
}
