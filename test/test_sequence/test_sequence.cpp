#include <string.h>
#include <unity.h>
#include "sequence.h"

static const uint16_t N = kPlanPixels;
static Rgb frame[N];

// Minimal PLP1 builder for tests: effect steps and uniform frame steps.
static uint8_t buf[16 + 8 * kFrameStepBytes];
static uint32_t len;
static uint8_t steps;

static void put8(uint8_t v) { buf[len++] = v; }
static void put16(uint16_t v) { put8(v & 0xFF); put8(v >> 8); }

static void begin(bool loop) {
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "PLP1", 4);
    buf[8] = loop ? 1 : 0;
    len = 16;
    steps = 0;
}

static void addSolid(Rgb c, uint16_t ms) {
    put8(1); put8(uint8_t(EffectId::Solid)); put16(ms); put8(255); put8(0); put8(16);
    put8(c.r); put8(c.g); put8(c.b);
    for (int i = 0; i < 6; i++) put8(0);
    steps++;
}

static void addFrame(Rgb c, uint16_t ms, uint16_t fadeMs, uint16_t delayMs) {
    put8(2); put16(ms);
    for (uint16_t i = 0; i < N; i++) { put8(c.r); put8(c.g); put8(c.b); put16(fadeMs); put16(delayMs); }
    steps++;
}

static Plan finish(PlanIndex& index, MemorySource*& src) {
    buf[9] = steps;
    uint32_t crc = crc32(buf + 16, len - 16);
    buf[12] = crc & 0xFF; buf[13] = (crc >> 8) & 0xFF; buf[14] = (crc >> 16) & 0xFF; buf[15] = crc >> 24;
    static MemorySource* held = nullptr;
    delete held;
    held = new MemorySource(buf, len);
    src = held;
    TEST_ASSERT_TRUE(planIndex(*src, index));
    return Plan{src, &index};
}

static void fillFrame(Rgb c) { for (uint16_t i = 0; i < N; i++) frame[i] = c; }

void setUp() { fillFrame(Rgb{0, 0, 0}); }
void tearDown() {}

void test_cut_frame_shows_its_colours_at_once() {
    static PlanIndex index; MemorySource* src;
    begin(false); addFrame(Rgb{10, 20, 30}, 500, 0, 0);
    SequenceRenderer r;
    r.start(finish(index, src), 1000, 1);
    TEST_ASSERT_TRUE(r.render(1000, frame, N));
    TEST_ASSERT_EQUAL_UINT8(10, frame[0].r);
    TEST_ASSERT_EQUAL_UINT8(30, frame[99].b);
}

void test_crossfade_interpolates_from_the_snapshot() {
    static PlanIndex index; MemorySource* src;
    begin(false); addFrame(Rgb{200, 100, 0}, 500, 100, 0);
    fillFrame(Rgb{0, 0, 200});
    SequenceRenderer r;
    r.start(finish(index, src), 0, 1);
    r.render(0, frame, N);
    r.render(50, frame, N);
    TEST_ASSERT_EQUAL_UINT8(100, frame[7].r);
    TEST_ASSERT_EQUAL_UINT8(50, frame[7].g);
    TEST_ASSERT_EQUAL_UINT8(100, frame[7].b);
    r.render(100, frame, N);
    TEST_ASSERT_EQUAL_UINT8(200, frame[7].r);
    TEST_ASSERT_EQUAL_UINT8(0, frame[7].b);
}

void test_pixel_holds_snapshot_until_its_delay() {
    static PlanIndex index; MemorySource* src;
    begin(false); addFrame(Rgb{255, 0, 0}, 500, 0, 200);
    fillFrame(Rgb{0, 255, 0});
    SequenceRenderer r;
    r.start(finish(index, src), 0, 1);
    r.render(0, frame, N);
    r.render(199, frame, N);
    TEST_ASSERT_EQUAL_UINT8(255, frame[3].g);
    r.render(200, frame, N);
    TEST_ASSERT_EQUAL_UINT8(255, frame[3].r);
    TEST_ASSERT_EQUAL_UINT8(0, frame[3].g);
}

void test_frame_fades_from_previous_step_output() {
    static PlanIndex index; MemorySource* src;
    begin(false); addSolid(Rgb{200, 0, 0}, 100); addFrame(Rgb{0, 0, 200}, 400, 100, 0);
    SequenceRenderer r;
    r.start(finish(index, src), 0, 1);
    r.render(50, frame, N);   // solid red
    r.render(150, frame, N);  // halfway red -> blue
    TEST_ASSERT_EQUAL_UINT8(100, frame[0].r);
    TEST_ASSERT_EQUAL_UINT8(100, frame[0].b);
}

void test_snapshot_is_fixed_for_the_whole_step() {
    static PlanIndex index; MemorySource* src;
    begin(false); addFrame(Rgb{200, 0, 0}, 400, 200, 0);
    SequenceRenderer r;
    r.start(finish(index, src), 0, 1);
    r.render(0, frame, N);
    r.render(100, frame, N);  // 100 from black
    r.render(150, frame, N);  // must be 150 from black, not 3/4 of the way from the last output
    TEST_ASSERT_EQUAL_UINT8(150, frame[0].r);
}

void test_one_shot_ends_and_leaves_frame_alone() {
    static PlanIndex index; MemorySource* src;
    begin(false); addSolid(Rgb{1, 2, 3}, 100);
    SequenceRenderer r;
    r.start(finish(index, src), 0, 1);
    TEST_ASSERT_TRUE(r.render(99, frame, N));
    frame[0] = Rgb{9, 9, 9};
    TEST_ASSERT_FALSE(r.render(100, frame, N));
    TEST_ASSERT_EQUAL_UINT8(9, frame[0].r);
}

void test_loop_restart_resnapshots() {
    static PlanIndex index; MemorySource* src;
    begin(true); addSolid(Rgb{0, 200, 0}, 100); addFrame(Rgb{200, 0, 0}, 100, 100, 0);
    SequenceRenderer r;
    r.start(finish(index, src), 0, 1);
    r.render(50, frame, N);    // green
    r.render(150, frame, N);   // green -> red halfway
    TEST_ASSERT_EQUAL_UINT8(100, frame[0].g);
    r.render(250, frame, N);   // loop 2: solid green again
    TEST_ASSERT_EQUAL_UINT8(200, frame[0].g);
    r.render(350, frame, N);   // halfway again, from green
    TEST_ASSERT_EQUAL_UINT8(100, frame[0].g);
    TEST_ASSERT_EQUAL_UINT8(100, frame[0].r);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_cut_frame_shows_its_colours_at_once);
    RUN_TEST(test_crossfade_interpolates_from_the_snapshot);
    RUN_TEST(test_pixel_holds_snapshot_until_its_delay);
    RUN_TEST(test_frame_fades_from_previous_step_output);
    RUN_TEST(test_snapshot_is_fixed_for_the_whole_step);
    RUN_TEST(test_one_shot_ends_and_leaves_frame_alone);
    RUN_TEST(test_loop_restart_resnapshots);
    return UNITY_END();
}
