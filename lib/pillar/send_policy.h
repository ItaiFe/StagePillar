#pragma once
#include <stdint.h>

enum class SendResult : uint8_t { Ok, Failed, Unreachable };

// HTTPClient's HTTPC_ERROR_READ_TIMEOUT: the request was sent but no reply came in time.
constexpr int kHttpReadTimeout = -11;

// How a finished POST is reported. A read timeout counts as sent: the server runs the
// whole device sequence before replying, so a slow reply still means the press arrived,
// and showing it as failed would invite a duplicate press.
inline SendResult classifySend(int httpCode) {
    if ((httpCode >= 200 && httpCode < 300) || httpCode == kHttpReadTimeout) return SendResult::Ok;
    if (httpCode < 0) return SendResult::Unreachable;
    return SendResult::Failed;
}

// Gestures that waited in the queue longer than this (behind a slow POST) are dropped,
// so a backlog never fires late.
constexpr uint32_t kMaxGestureAgeMs = 1000;

inline bool gestureStale(uint32_t queuedAtMs, uint32_t nowMs) {
    return nowMs - queuedAtMs > kMaxGestureAgeMs;
}
