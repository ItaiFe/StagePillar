// Parses the golden PLP1 file produced by StageController's compiler
// (backend/tests/fixtures/pillar_golden.bin), so both sides agree byte for byte.
#include <unity.h>
#include "golden_bin.h"
#include "plp.h"

static MemorySource src(pillar_golden_bin, pillar_golden_bin_len);
static PlanIndex index;
static FramePixel px[kPlanPixels];

void setUp() { TEST_ASSERT_TRUE(planIndex(src, index)); }
void tearDown() {}

void test_header() {
    TEST_ASSERT_EQUAL_UINT32(2876, pillar_golden_bin_len);
    TEST_ASSERT_EQUAL_UINT32(42, index.version);
    TEST_ASSERT_TRUE(index.loop);
    TEST_ASSERT_EQUAL_UINT8(7, index.stepCount);
    TEST_ASSERT_EQUAL_UINT32(2100 + 500 + 20480 + 300 + 800 + 400 + 1000, index.totalMs);
}

void test_step_kinds() {
    const StepKind kinds[] = {StepKind::Effect, StepKind::Effect, StepKind::Effect, StepKind::Frame,
                              StepKind::Frame, StepKind::Frame, StepKind::Frame};
    for (int i = 0; i < 7; i++) TEST_ASSERT_EQUAL_INT((int)kinds[i], (int)index.kind[i]);
}

void test_comet_step() {
    EffectStep s;
    TEST_ASSERT_TRUE(readEffectStep(src, index, 0, s));
    TEST_ASSERT_EQUAL_INT((int)EffectId::Comet, (int)s.effect);
    TEST_ASSERT_EQUAL_UINT16(2100, s.durationMs);
    TEST_ASSERT_EQUAL_UINT8(255, s.brightness);
    TEST_ASSERT_EQUAL_INT((int)Direction::Bounce, (int)s.direction);
    TEST_ASSERT_EQUAL_UINT8(24, s.speedX16);
    TEST_ASSERT_EQUAL_UINT8(0xC0, s.colors[0].b);
    TEST_ASSERT_EQUAL_UINT8(0xE5, s.colors[1].g);
    TEST_ASSERT_EQUAL_UINT8(0xB0, s.colors[2].g);
}

void test_band_and_rainbow_steps() {
    EffectStep s;
    TEST_ASSERT_TRUE(readEffectStep(src, index, 1, s));
    TEST_ASSERT_EQUAL_INT((int)EffectId::Band, (int)s.effect);
    TEST_ASSERT_EQUAL_UINT8(200, s.brightness);
    TEST_ASSERT_EQUAL_INT((int)Direction::Down, (int)s.direction);
    TEST_ASSERT_EQUAL_UINT8(4, s.speedX16);
    TEST_ASSERT_TRUE(readEffectStep(src, index, 2, s));
    TEST_ASSERT_EQUAL_INT((int)EffectId::Rainbow, (int)s.effect);
    TEST_ASSERT_EQUAL_UINT16(20480, s.durationMs);
    TEST_ASSERT_EQUAL_UINT8(64, s.speedX16);
}

static void assertPixel(uint8_t step, uint16_t i, uint8_t r, uint8_t g, uint8_t b, uint16_t fade, uint16_t delay) {
    TEST_ASSERT_TRUE(readFramePixels(src, index, step, px));
    TEST_ASSERT_EQUAL_UINT8(r, px[i].color.r);
    TEST_ASSERT_EQUAL_UINT8(g, px[i].color.g);
    TEST_ASSERT_EQUAL_UINT8(b, px[i].color.b);
    TEST_ASSERT_EQUAL_UINT16(fade, px[i].fadeMs);
    TEST_ASSERT_EQUAL_UINT16(delay, px[i].delayMs);
}

void test_cut_frame() { assertPixel(3, 0, 0, 255, 0, 0, 0); }
void test_crossfade_frame_bakes_brightness() { assertPixel(4, 10, 128, 64, 32, 400, 0); }
void test_wipe_frame_is_trimmed_to_the_step() {
    assertPixel(5, 0, 0, 0, 255, 40, 0);
    assertPixel(5, 99, 0, 0, 255, 4, 396);
}
void test_custom_frame() { assertPixel(6, 50, 205, 205, 205, 250, 200); }

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_header);
    RUN_TEST(test_step_kinds);
    RUN_TEST(test_comet_step);
    RUN_TEST(test_band_and_rainbow_steps);
    RUN_TEST(test_cut_frame);
    RUN_TEST(test_crossfade_frame_bakes_brightness);
    RUN_TEST(test_wipe_frame_is_trimmed_to_the_step);
    RUN_TEST(test_custom_frame);
    return UNITY_END();
}
