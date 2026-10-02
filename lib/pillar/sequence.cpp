#include "sequence.h"

static uint8_t lerp8(uint8_t a, uint8_t b, uint32_t t, uint32_t total) {
    return uint8_t(int32_t(a) + (int32_t(b) - int32_t(a)) * int32_t(t) / int32_t(total));
}

void SequenceRenderer::start(Plan plan, uint32_t nowMs, uint32_t seed) {
    plan_ = plan;
    startMs_ = nowMs;
    seed_ = seed;
    loadedStep_ = -1;
}

void SequenceRenderer::load(uint8_t step, const Rgb* frame, uint16_t count) {
    const PlanIndex& index = *plan_.index;
    if (index.kind[step] == StepKind::Effect) {
        loadOk_ = readEffectStep(*plan_.src, index, step, effect_);
    } else {
        loadOk_ = readFramePixels(*plan_.src, index, step, pixels_);
        for (uint16_t i = 0; i < count && i < kPlanPixels; i++) snapshot_[i] = frame[i];
    }
}

bool SequenceRenderer::render(uint32_t nowMs, Rgb* frame, uint16_t count) {
    if (!plan_.index || plan_.index->totalMs == 0) return false;
    const PlanIndex& index = *plan_.index;

    uint32_t t = nowMs - startMs_;
    uint32_t pass = t / index.totalMs;
    if (!index.loop && pass > 0) return false;
    t %= index.totalMs;

    uint8_t step = 0;
    while (step + 1 < index.stepCount && t >= index.durationMs[step]) t -= index.durationMs[step++];

    if (step != loadedStep_ || pass != loadedPass_) {
        load(step, frame, count);
        loadedStep_ = step;
        loadedPass_ = pass;
    }

    if (!loadOk_) {
        for (uint16_t i = 0; i < count; i++) frame[i] = Rgb{0, 0, 0};
        return true;
    }

    if (index.kind[step] == StepKind::Effect) {
        renderStep(effect_, t, seed_, frame, count);
        return true;
    }

    // Each pixel holds the snapshot until its delay, then fades linearly to its colour.
    for (uint16_t i = 0; i < count && i < kPlanPixels; i++) {
        const FramePixel& px = pixels_[i];
        if (t < px.delayMs) {
            frame[i] = snapshot_[i];
        } else if (t < uint32_t(px.delayMs) + px.fadeMs) {
            uint32_t into = t - px.delayMs;
            frame[i] = Rgb{lerp8(snapshot_[i].r, px.color.r, into, px.fadeMs),
                           lerp8(snapshot_[i].g, px.color.g, into, px.fadeMs),
                           lerp8(snapshot_[i].b, px.color.b, into, px.fadeMs)};
        } else {
            frame[i] = px.color;
        }
    }
    return true;
}
