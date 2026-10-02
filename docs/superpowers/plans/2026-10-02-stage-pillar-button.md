# StagePillar Button Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** ESP32 firmware for one stage button. It turns 1–4 presses and a long press into HTTP POSTs to the StageController server on the RPi, and it can be updated over the air (OTA).

**Architecture:** A pure C++ `GestureDetector`, together with the gesture→action and LED-pattern tables, lives in `lib/pillar/`. It has no Arduino dependencies, so it is unit-tested on the Mac with `pio test -e native`. The Arduino side lives in `src/`. `loop()` samples the button, feeds the detector and pushes gestures onto a FreeRTOS queue. A separate sender task does the blocking HTTP POST, so a slow server never stalls button reading or OTA.

**Tech Stack:** PlatformIO 6.1, `espressif32` platform (Arduino-ESP32 core 2.0.17), Unity for native tests, `HTTPClient`, `ESPmDNS`, `ArduinoOTA`.

**Spec:** `docs/superpowers/specs/2026-10-02-stage-pillar-button-design.md`

## Global Constraints

- Gesture → action: 1 press = `start`, 2 = `claps`, 3 = `special`, 4 = `skip`, long press = `stop`, 5+ presses = nothing.
- Request: `POST http://<pi>:8000/api/buttons/press/<action>`, empty body. No server changes.
- Timing: debounce 30 ms, gap 400 ms, long press 1500 ms (fires while held; earlier taps in that gesture are discarded; the release after it emits nothing).
- Button: GPIO 4 to GND, `INPUT_PULLUP`, pressed = LOW. Status LED: GPIO 2 (active high).
- POST timeout 2 s. No retries. On a connection failure, forget the cached server IP so the next send looks it up again.
- WiFi down → gesture dropped and logged. Queue length 4; when full, the new gesture is dropped. During OTA, gestures are ignored.
- Server address: mDNS `flamingods.local`, cached; fallback `SERVER_FALLBACK_IP` from secrets.
- OTA hostname `stage-pillar`; OTA password from `include/secrets.h`; the `esp32dev-ota` env reads auth from env var `ESP_OTA_PASSWORD`.
- `include/secrets.h` is gitignored (already in `.gitignore`); `include/secrets.example.h` is committed.
- LED: one short blink = 2xx; three fast blinks = failure; slow blink = WiFi down; off = idle.
- Serial at 115200 baud.

Deviations from the spec's file table, both made for buildability:
- The pure code lives in `lib/pillar/` rather than `src/`, so native tests can build it without `main.cpp`.
- The secret is named `SERVER_MDNS_NAME = "flamingods"` (the bare name `MDNS.queryHost` expects) instead of `SERVER_HOST`.

## Review Focus

1. **Button held at boot or stuck pressed**: exactly one `stop`, never repeated. Pinned by `test_held_forever_emits_one_long` (Task 1).
2. **`millis()` rollover after 49.7 days**: gestures are still detected across the wrap. Pinned by `test_works_across_millis_rollover` (Task 1).
3. **A press that starts just before the gap ends**: it continues the gesture instead of splitting it. Pinned by `test_press_399ms_after_release_continues_gesture` (Task 1).
4. **Pi unreachable**: button reading keeps working, failure blinks, and the next send looks up the address again. Pinned by the manual unreachable-server check (Task 5, Step 6).
5. **Fresh clone without `secrets.h`**: the build fails with a clear message instead of obscure undefined-macro errors. Pinned by Task 3, Step 2.

---

## File Structure

```
StagePillar/
├── platformio.ini                 # esp32dev, esp32dev-ota, native envs
├── README.md                      # wiring, gestures, flashing
├── include/
│   ├── config.h                   # pins, timeouts, hostname; pulls in secrets.h
│   ├── secrets.example.h          # committed template
│   └── secrets.h                  # gitignored, real values
├── lib/pillar/                    # pure C++, host-testable
│   ├── gesture.h / gesture.cpp    # GestureDetector
│   ├── actions.h                  # actionFor(Gesture)
│   └── led_pattern.h              # ledLevel(...)
├── src/
│   ├── main.cpp                   # wiring only
│   ├── status_led.h / .cpp        # drives GPIO 2 from ledLevel
│   ├── net.h / net.cpp            # WiFi + server IP lookup/cache
│   ├── ota.h / ota.cpp            # ArduinoOTA (also starts mDNS)
│   └── sender.h / sender.cpp      # queue + POST task
└── test/
    ├── test_gesture/test_gesture.cpp
    └── test_tables/test_tables.cpp
```

---

### Task 1: Project scaffold and GestureDetector

**Files:**
- Create: `platformio.ini`
- Create: `lib/pillar/gesture.h`, `lib/pillar/gesture.cpp`
- Test: `test/test_gesture/test_gesture.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `enum class Gesture : uint8_t { None, Single, Double, Triple, Quad, Long };`
  - `struct GestureTiming { uint32_t debounceMs = 30; uint32_t gapMs = 400; uint32_t longMs = 1500; };`
  - `class GestureDetector { public: explicit GestureDetector(GestureTiming timing = GestureTiming()); Gesture update(bool pressed, uint32_t nowMs); };`

- [ ] **Step 1: Create `platformio.ini`**

```ini
[platformio]
default_envs = esp32dev

[esp32_common]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
build_flags =
    -DCORE_DEBUG_LEVEL=2
    -DFIRMWARE_VERSION=\"1.0.0\"

[env:esp32dev]
extends = esp32_common

[env:esp32dev-ota]
extends = esp32_common
upload_protocol = espota
upload_port = stage-pillar.local
upload_flags =
    --auth=${sysenv.ESP_OTA_PASSWORD}

[env:native]
platform = native
test_framework = unity
build_flags = -std=c++17
```

- [ ] **Step 2: Write the failing tests** in `test/test_gesture/test_gesture.cpp`

```cpp
#include <unity.h>
#include <utility>
#include <vector>
#include "gesture.h"

// Drives the detector one millisecond at a time and records each gesture with its time.
struct Sim {
    GestureDetector detector;
    uint32_t t;
    std::vector<std::pair<Gesture, uint32_t>> out;

    explicit Sim(uint32_t start = 0) : t(start) {}

    void hold(bool pressed, uint32_t ms) {
        for (uint32_t i = 0; i < ms; i++, t++) {
            Gesture g = detector.update(pressed, t);
            if (g != Gesture::None) out.push_back({g, t});
        }
    }

    void tap() {
        hold(true, 100);
        hold(false, 100);
    }

    void taps(int n) {
        hold(false, 10);
        for (int i = 0; i < n; i++) tap();
        hold(false, 1000);
    }
};

static void assertOnly(const Sim& s, Gesture expected) {
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, s.out.size(), "expected exactly one gesture");
    TEST_ASSERT_EQUAL_INT((int)expected, (int)s.out[0].first);
}

void setUp() {}
void tearDown() {}

void test_one_tap_is_single() { Sim s; s.taps(1); assertOnly(s, Gesture::Single); }
void test_two_taps_is_double() { Sim s; s.taps(2); assertOnly(s, Gesture::Double); }
void test_three_taps_is_triple() { Sim s; s.taps(3); assertOnly(s, Gesture::Triple); }
void test_four_taps_is_quad() { Sim s; s.taps(4); assertOnly(s, Gesture::Quad); }

void test_five_taps_is_ignored() {
    Sim s;
    s.taps(5);
    TEST_ASSERT_EQUAL_UINT32(0, s.out.size());
}

void test_detector_recovers_after_five_taps() {
    Sim s;
    s.taps(5);
    s.taps(1);
    assertOnly(s, Gesture::Single);
}

void test_bounce_shorter_than_debounce_is_ignored() {
    Sim s;
    s.hold(false, 10);
    s.hold(true, 20);
    s.hold(false, 1000);
    TEST_ASSERT_EQUAL_UINT32(0, s.out.size());
}

void test_bounce_inside_a_press_counts_once() {
    Sim s;
    s.hold(false, 10);
    s.hold(true, 50);
    s.hold(false, 10);
    s.hold(true, 50);
    s.hold(false, 1000);
    assertOnly(s, Gesture::Single);
}

void test_gap_ends_gesture_exactly_400ms_after_release() {
    Sim s;
    s.hold(false, 10);
    s.hold(true, 100);
    uint32_t release = s.t;
    s.hold(false, 400);  // ticks release .. release+399
    TEST_ASSERT_EQUAL_UINT32(0, s.out.size());
    s.hold(false, 1);    // tick release+400
    assertOnly(s, Gesture::Single);
    TEST_ASSERT_EQUAL_UINT32(release + 400, s.out[0].second);
}

void test_press_399ms_after_release_continues_gesture() {
    Sim s;
    s.hold(false, 10);
    s.hold(true, 100);
    s.hold(false, 399);
    s.hold(true, 100);
    s.hold(false, 1000);
    assertOnly(s, Gesture::Double);
}

void test_long_press_fires_at_1500ms_while_held() {
    Sim s;
    s.hold(false, 10);
    uint32_t pressed = s.t;
    s.hold(true, 2000);
    assertOnly(s, Gesture::Long);
    TEST_ASSERT_EQUAL_UINT32(pressed + 1500, s.out[0].second);
    s.hold(false, 1000);
    assertOnly(s, Gesture::Long);  // release emits nothing
}

void test_hold_just_under_long_is_single() {
    Sim s;
    s.hold(false, 10);
    s.hold(true, 1499);
    s.hold(false, 1000);
    assertOnly(s, Gesture::Single);
}

void test_taps_then_long_press_is_only_long() {
    Sim s;
    s.hold(false, 10);
    s.tap();
    s.tap();
    s.hold(true, 2000);
    s.hold(false, 1000);
    assertOnly(s, Gesture::Long);
}

void test_held_forever_emits_one_long() {
    Sim s;
    s.hold(false, 10);
    s.hold(true, 10000);
    s.hold(false, 1000);
    assertOnly(s, Gesture::Long);
}

void test_tap_after_long_press_is_single() {
    Sim s;
    s.hold(false, 10);
    s.hold(true, 2000);
    s.hold(false, 1000);
    s.taps(1);
    TEST_ASSERT_EQUAL_UINT32(2, s.out.size());
    TEST_ASSERT_EQUAL_INT((int)Gesture::Single, (int)s.out[1].first);
}

void test_works_across_millis_rollover() {
    Sim s(0xFFFFFFFFu - 50);
    s.taps(2);
    assertOnly(s, Gesture::Double);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_one_tap_is_single);
    RUN_TEST(test_two_taps_is_double);
    RUN_TEST(test_three_taps_is_triple);
    RUN_TEST(test_four_taps_is_quad);
    RUN_TEST(test_five_taps_is_ignored);
    RUN_TEST(test_detector_recovers_after_five_taps);
    RUN_TEST(test_bounce_shorter_than_debounce_is_ignored);
    RUN_TEST(test_bounce_inside_a_press_counts_once);
    RUN_TEST(test_gap_ends_gesture_exactly_400ms_after_release);
    RUN_TEST(test_press_399ms_after_release_continues_gesture);
    RUN_TEST(test_long_press_fires_at_1500ms_while_held);
    RUN_TEST(test_hold_just_under_long_is_single);
    RUN_TEST(test_taps_then_long_press_is_only_long);
    RUN_TEST(test_held_forever_emits_one_long);
    RUN_TEST(test_tap_after_long_press_is_single);
    RUN_TEST(test_works_across_millis_rollover);
    return UNITY_END();
}
```

- [ ] **Step 3: Run the tests to verify they fail**

Run: `pio test -e native -f test_gesture`
Expected: build FAIL, `gesture.h: No such file or directory`.

- [ ] **Step 4: Write `lib/pillar/gesture.h`**

```cpp
#pragma once
#include <stdint.h>

enum class Gesture : uint8_t { None, Single, Double, Triple, Quad, Long };

struct GestureTiming {
    uint32_t debounceMs = 30;
    uint32_t gapMs = 400;
    uint32_t longMs = 1500;
};

// Turns a raw button level, sampled every loop, into press gestures.
// Pure logic with no Arduino dependencies so it can be unit-tested on the host.
class GestureDetector {
public:
    explicit GestureDetector(GestureTiming timing = GestureTiming());

    // pressed: raw level, true while the button is down. nowMs: millis().
    // Returns the gesture on the call where it completes, else Gesture::None.
    Gesture update(bool pressed, uint32_t nowMs);

private:
    GestureTiming timing_;
    bool raw_ = false;          // last sampled level
    uint32_t rawSince_ = 0;     // when raw_ last changed
    bool stable_ = false;       // debounced level
    uint32_t stableSince_ = 0;  // edge time of the last debounced change
    uint8_t count_ = 0;         // presses in the current gesture
    bool longFired_ = false;    // Long already emitted for the current hold
};
```

- [ ] **Step 5: Write `lib/pillar/gesture.cpp`**

```cpp
#include "gesture.h"

// Counts above 4 only need to be recognised as "too many", so stop counting here.
static const uint8_t kMaxCount = 5;

static Gesture gestureForCount(uint8_t count) {
    switch (count) {
        case 1: return Gesture::Single;
        case 2: return Gesture::Double;
        case 3: return Gesture::Triple;
        case 4: return Gesture::Quad;
        default: return Gesture::None;
    }
}

GestureDetector::GestureDetector(GestureTiming timing) : timing_(timing) {}

Gesture GestureDetector::update(bool pressed, uint32_t nowMs) {
    if (pressed != raw_) {
        raw_ = pressed;
        rawSince_ = nowMs;
    }

    if (raw_ != stable_ && nowMs - rawSince_ >= timing_.debounceMs) {
        stable_ = raw_;
        stableSince_ = rawSince_;
        if (!stable_) {
            if (longFired_) {
                longFired_ = false;
            } else if (count_ < kMaxCount) {
                count_++;
            }
        }
    }

    if (stable_ && !longFired_ && nowMs - stableSince_ >= timing_.longMs) {
        longFired_ = true;
        count_ = 0;
        return Gesture::Long;
    }

    // raw_ must also be up: a press that has started but not yet debounced keeps the gesture open.
    if (!stable_ && !raw_ && count_ > 0 && nowMs - stableSince_ >= timing_.gapMs) {
        uint8_t count = count_;
        count_ = 0;
        return gestureForCount(count);
    }

    return Gesture::None;
}
```

- [ ] **Step 6: Run the tests to verify they pass**

Run: `pio test -e native -f test_gesture`
Expected: `16 Tests 0 Failures 0 Ignored`, PASSED.

- [ ] **Step 7: Commit**

```bash
git add platformio.ini lib/pillar/gesture.h lib/pillar/gesture.cpp test/test_gesture/test_gesture.cpp
git commit -m "Add GestureDetector with native tests"
```

---

### Task 2: Gesture→action and LED pattern tables

**Files:**
- Create: `lib/pillar/actions.h`, `lib/pillar/led_pattern.h`
- Test: `test/test_tables/test_tables.cpp`

**Interfaces:**
- Consumes: `Gesture` from `gesture.h` (Task 1).
- Produces:
  - `inline const char* actionFor(Gesture g);` returns `nullptr` for `Gesture::None`.
  - `enum class Flash : uint8_t { None, Ok, Fail };`
  - `inline bool ledLevel(Flash flash, uint32_t msSinceFlash, bool wifiConnected, uint32_t nowMs);` returns true when the LED should be lit.

- [ ] **Step 1: Write the failing tests** in `test/test_tables/test_tables.cpp`

```cpp
#include <unity.h>
#include "actions.h"
#include "led_pattern.h"

void setUp() {}
void tearDown() {}

void test_action_mapping() {
    TEST_ASSERT_EQUAL_STRING("start", actionFor(Gesture::Single));
    TEST_ASSERT_EQUAL_STRING("claps", actionFor(Gesture::Double));
    TEST_ASSERT_EQUAL_STRING("special", actionFor(Gesture::Triple));
    TEST_ASSERT_EQUAL_STRING("skip", actionFor(Gesture::Quad));
    TEST_ASSERT_EQUAL_STRING("stop", actionFor(Gesture::Long));
    TEST_ASSERT_NULL(actionFor(Gesture::None));
}

void test_led_ok_is_one_100ms_blink() {
    TEST_ASSERT_TRUE(ledLevel(Flash::Ok, 0, true, 5000));
    TEST_ASSERT_TRUE(ledLevel(Flash::Ok, 99, true, 5000));
    TEST_ASSERT_FALSE(ledLevel(Flash::Ok, 100, true, 5000));
}

void test_led_fail_is_three_fast_blinks() {
    // 60 ms on / 60 ms off, three times, over 360 ms.
    const uint32_t onTimes[] = {0, 59, 120, 179, 240, 299};
    const uint32_t offTimes[] = {60, 119, 180, 239, 300, 359, 360, 1000};
    for (uint32_t ms : onTimes) TEST_ASSERT_TRUE(ledLevel(Flash::Fail, ms, true, 5000));
    for (uint32_t ms : offTimes) TEST_ASSERT_FALSE(ledLevel(Flash::Fail, ms, true, 5000));
}

void test_led_slow_blink_when_wifi_down() {
    TEST_ASSERT_TRUE(ledLevel(Flash::None, 0, false, 0));
    TEST_ASSERT_TRUE(ledLevel(Flash::None, 0, false, 499));
    TEST_ASSERT_FALSE(ledLevel(Flash::None, 0, false, 500));
    TEST_ASSERT_FALSE(ledLevel(Flash::None, 0, false, 999));
    TEST_ASSERT_TRUE(ledLevel(Flash::None, 0, false, 1000));
}

void test_led_off_when_idle_and_connected() {
    TEST_ASSERT_FALSE(ledLevel(Flash::None, 0, true, 0));
    TEST_ASSERT_FALSE(ledLevel(Flash::Ok, 5000, true, 5000));
}

void test_led_flash_overrides_wifi_blink() {
    // At nowMs=500 the slow blink would be off, but a fresh Ok flash shows.
    TEST_ASSERT_TRUE(ledLevel(Flash::Ok, 10, false, 500));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_action_mapping);
    RUN_TEST(test_led_ok_is_one_100ms_blink);
    RUN_TEST(test_led_fail_is_three_fast_blinks);
    RUN_TEST(test_led_slow_blink_when_wifi_down);
    RUN_TEST(test_led_off_when_idle_and_connected);
    RUN_TEST(test_led_flash_overrides_wifi_blink);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `pio test -e native -f test_tables`
Expected: build FAIL, `actions.h: No such file or directory`.

- [ ] **Step 3: Write `lib/pillar/actions.h`**

```cpp
#pragma once
#include "gesture.h"

// Server action for each gesture, sent as POST /api/buttons/press/<action>.
// Returns nullptr for gestures that send nothing.
inline const char* actionFor(Gesture g) {
    switch (g) {
        case Gesture::Single: return "start";
        case Gesture::Double: return "claps";
        case Gesture::Triple: return "special";
        case Gesture::Quad:   return "skip";
        case Gesture::Long:   return "stop";
        default:              return nullptr;
    }
}
```

- [ ] **Step 4: Write `lib/pillar/led_pattern.h`**

```cpp
#pragma once
#include <stdint.h>

enum class Flash : uint8_t { None, Ok, Fail };

// Whether the status LED should be lit. A recent flash takes priority over the
// slow WiFi-down blink.
inline bool ledLevel(Flash flash, uint32_t msSinceFlash, bool wifiConnected, uint32_t nowMs) {
    if (flash == Flash::Ok && msSinceFlash < 100) return true;
    if (flash == Flash::Fail && msSinceFlash < 360) return (msSinceFlash / 60) % 2 == 0;
    if (!wifiConnected) return (nowMs / 500) % 2 == 0;
    return false;
}
```

- [ ] **Step 5: Run all native tests to verify they pass**

Run: `pio test -e native`
Expected: both `test_gesture` (16) and `test_tables` (6) PASSED.

- [ ] **Step 6: Commit**

```bash
git add lib/pillar/actions.h lib/pillar/led_pattern.h test/test_tables/test_tables.cpp
git commit -m "Add gesture-to-action and LED pattern tables"
```

---

### Task 3: Firmware skeleton (config, secrets, LED, WiFi, OTA, main)

After this task the board connects to WiFi, accepts OTA, shows the WiFi LED status and logs gestures over serial. It does not send anything yet.

**Files:**
- Create: `include/config.h`, `include/secrets.example.h`, `include/secrets.h` (local only, gitignored)
- Create: `src/status_led.h`, `src/status_led.cpp`
- Create: `src/net.h`, `src/net.cpp`
- Create: `src/ota.h`, `src/ota.cpp`
- Create: `src/main.cpp`

**Interfaces:**
- Consumes: `GestureDetector`, `Gesture` (Task 1); `actionFor`, `Flash`, `ledLevel` (Task 2).
- Produces:
  - `config.h` constants: `BUTTON_PIN`, `LED_PIN`, `DEVICE_HOSTNAME`, `HTTP_TIMEOUT_MS`, `MDNS_TIMEOUT_MS`, `GESTURE_QUEUE_LEN`.
  - `secrets.h` macros: `WIFI_SSID`, `WIFI_PASSWORD`, `OTA_PASSWORD`, `SERVER_MDNS_NAME`, `SERVER_PORT`, `SERVER_FALLBACK_IP`.
  - `void statusLedBegin(); void statusLedFlash(Flash flash); void statusLedUpdate(uint32_t nowMs, bool wifiConnected);`
  - `void netBegin(); bool netConnected(); IPAddress netServerIp(); void netForgetServerIp();`
  - `void otaLoop(bool wifiConnected); bool otaActive();`

- [ ] **Step 1: Write `include/config.h` and `include/secrets.example.h`**

`include/config.h`:

```cpp
#pragma once
#include <stdint.h>

#if !__has_include("secrets.h")
#error "Missing include/secrets.h: copy include/secrets.example.h to include/secrets.h and fill it in"
#endif
#include "secrets.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

constexpr uint8_t BUTTON_PIN = 4;   // switch to GND, internal pull-up
constexpr uint8_t LED_PIN = 2;      // onboard LED, active high
constexpr const char* DEVICE_HOSTNAME = "stage-pillar";
constexpr uint16_t HTTP_TIMEOUT_MS = 2000;
constexpr uint32_t MDNS_TIMEOUT_MS = 1500;
constexpr uint8_t GESTURE_QUEUE_LEN = 4;
```

`include/secrets.example.h`:

```cpp
#pragma once
// Copy to include/secrets.h (gitignored) and fill in real values.

#define WIFI_SSID "Flamingods"
#define WIFI_PASSWORD "change-me"

// Must match the ESP_OTA_PASSWORD env var used by `pio run -e esp32dev-ota -t upload`.
#define OTA_PASSWORD "change-me"

// The StageController Pi, resolved via mDNS as <name>.local.
#define SERVER_MDNS_NAME "flamingods"
#define SERVER_PORT 8000
// Used when the mDNS lookup fails.
#define SERVER_FALLBACK_IP "192.168.0.105"
```

- [ ] **Step 2: Write `src/main.cpp` and check that a missing `secrets.h` fails clearly**

`src/main.cpp`:

```cpp
#include <Arduino.h>
#include "actions.h"
#include "config.h"
#include "gesture.h"
#include "net.h"
#include "ota.h"
#include "status_led.h"

static GestureDetector detector;

void setup() {
    Serial.begin(115200);
    Serial.printf("\nStagePillar button %s\n", FIRMWARE_VERSION);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    statusLedBegin();
    netBegin();
}

void loop() {
    uint32_t now = millis();
    bool wifi = netConnected();

    Gesture gesture = detector.update(digitalRead(BUTTON_PIN) == LOW, now);
    if (gesture != Gesture::None) {
        Serial.printf("Gesture -> %s\n", actionFor(gesture));
    }

    otaLoop(wifi);
    statusLedUpdate(now, wifi);
    delay(1);
}
```

Run: `pio run -e esp32dev`
Expected: FAIL with `#error "Missing include/secrets.h: copy include/secrets.example.h ..."`.

- [ ] **Step 3: Write `src/status_led.h` and `src/status_led.cpp`**

`src/status_led.h`:

```cpp
#pragma once
#include <stdint.h>
#include "led_pattern.h"

void statusLedBegin();
// Starts a flash pattern. Safe to call from any task.
void statusLedFlash(Flash flash);
// Renders the current pattern. Call from loop().
void statusLedUpdate(uint32_t nowMs, bool wifiConnected);
```

`src/status_led.cpp`:

```cpp
#include "status_led.h"
#include <Arduino.h>
#include "config.h"

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static Flash flash = Flash::None;
static uint32_t flashStart = 0;

void statusLedBegin() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
}

void statusLedFlash(Flash f) {
    portENTER_CRITICAL(&mux);
    flash = f;
    flashStart = millis();
    portEXIT_CRITICAL(&mux);
}

void statusLedUpdate(uint32_t nowMs, bool wifiConnected) {
    portENTER_CRITICAL(&mux);
    Flash f = flash;
    uint32_t start = flashStart;
    portEXIT_CRITICAL(&mux);
    digitalWrite(LED_PIN, ledLevel(f, nowMs - start, wifiConnected, nowMs) ? HIGH : LOW);
}
```

- [ ] **Step 4: Write `src/net.h` and `src/net.cpp`**

`src/net.h`:

```cpp
#pragma once
#include <IPAddress.h>

// Starts WiFi in the background; it reconnects automatically.
void netBegin();
bool netConnected();
// The Pi's address: an mDNS lookup of SERVER_MDNS_NAME, else SERVER_FALLBACK_IP.
// The result is cached. May block up to MDNS_TIMEOUT_MS; call only from the sender task.
IPAddress netServerIp();
// Forgets the cached address so the next netServerIp() looks it up again.
void netForgetServerIp();
```

`src/net.cpp`:

```cpp
#include "net.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include "config.h"

// mDNS itself is started by ArduinoOTA.begin() (see ota.cpp); starting it twice fails.
static IPAddress cachedIp;  // 0.0.0.0 = not resolved yet

void netBegin() {
    WiFi.setHostname(DEVICE_HOSTNAME);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("WiFi: connecting to %s\n", WIFI_SSID);
}

bool netConnected() {
    return WiFi.isConnected();
}

IPAddress netServerIp() {
    if (uint32_t(cachedIp) != 0) return cachedIp;

    IPAddress ip = MDNS.queryHost(SERVER_MDNS_NAME, MDNS_TIMEOUT_MS);
    if (uint32_t(ip) != 0) {
        Serial.printf("Server: %s.local is %s\n", SERVER_MDNS_NAME, ip.toString().c_str());
    } else {
        ip.fromString(SERVER_FALLBACK_IP);
        Serial.printf("Server: mDNS lookup failed, using %s\n", SERVER_FALLBACK_IP);
    }
    cachedIp = ip;
    return ip;
}

void netForgetServerIp() {
    cachedIp = IPAddress();
}
```

- [ ] **Step 5: Write `src/ota.h` and `src/ota.cpp`**

`src/ota.h`:

```cpp
#pragma once

// Starts ArduinoOTA (and mDNS) the first time WiFi is up, then services it.
// Call from loop().
void otaLoop(bool wifiConnected);
// True while an OTA upload is in progress.
bool otaActive();
```

`src/ota.cpp`:

```cpp
#include "ota.h"
#include <ArduinoOTA.h>
#include "config.h"

static bool started = false;
static volatile bool active = false;

static void otaBegin() {
    ArduinoOTA.setHostname(DEVICE_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([]() {
        active = true;
        Serial.println("OTA: update started");
    });
    ArduinoOTA.onEnd([]() { Serial.println("\nOTA: done, rebooting"); });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
        Serial.printf("OTA: %u%%\r", done * 100 / total);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        active = false;
        Serial.printf("\nOTA: error %u\n", error);
    });
    ArduinoOTA.begin();
    started = true;
    Serial.printf("OTA: ready at %s.local\n", DEVICE_HOSTNAME);
}

void otaLoop(bool wifiConnected) {
    if (!started) {
        if (wifiConnected) otaBegin();
        return;
    }
    ArduinoOTA.handle();
}

bool otaActive() {
    return active;
}
```

- [ ] **Step 6: Create the local `include/secrets.h`**

Copy `include/secrets.example.h` to `include/secrets.h`.
- Ask the user for the `Flamingods` WiFi password.
- Set `OTA_PASSWORD` to `"flamingods2024"`, which matches the board's current firmware, unless the user picks another.

Confirm git ignores it:

Run: `git check-ignore include/secrets.h`
Expected: prints `include/secrets.h`.

- [ ] **Step 7: Build**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`, no warnings from `src/` or `lib/pillar/`.

- [ ] **Step 8: Run native tests again** (the shared lib must still build on the host)

Run: `pio test -e native`
Expected: all PASSED.

- [ ] **Step 9: Commit** (never `secrets.h`)

```bash
git add include/config.h include/secrets.example.h src/
git status --short   # must not list include/secrets.h
git commit -m "Add firmware skeleton: WiFi, OTA, status LED, gesture logging"
```

---

### Task 4: Sender task, wired into the loop

**Files:**
- Create: `src/sender.h`, `src/sender.cpp`
- Modify: `src/main.cpp` (add `senderBegin()` in `setup()`, enqueue in `loop()`)

**Interfaces:**
- Consumes: `actionFor` (Task 2); `netConnected`, `netServerIp`, `netForgetServerIp`, `otaActive`, `statusLedFlash` (Task 3); `HTTP_TIMEOUT_MS`, `GESTURE_QUEUE_LEN`, `SERVER_PORT` (Task 3).
- Produces: `void senderBegin(); bool senderEnqueue(Gesture gesture);`

- [ ] **Step 1: Write `src/sender.h`**

```cpp
#pragma once
#include "gesture.h"

// Creates the gesture queue and the task that POSTs gestures to the server.
void senderBegin();
// Non-blocking. Returns false if the queue is full and the gesture was dropped.
bool senderEnqueue(Gesture gesture);
```

- [ ] **Step 2: Write `src/sender.cpp`**

```cpp
#include "sender.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include "actions.h"
#include "config.h"
#include "net.h"
#include "ota.h"
#include "status_led.h"

static QueueHandle_t queue;

static void send(Gesture gesture) {
    const char* action = actionFor(gesture);
    if (!action) return;
    if (otaActive()) {
        Serial.printf("Drop %s: OTA in progress\n", action);
        return;
    }
    if (!netConnected()) {
        Serial.printf("Drop %s: WiFi down\n", action);
        statusLedFlash(Flash::Fail);
        return;
    }

    String url = "http://" + netServerIp().toString() + ":" + String(SERVER_PORT) +
                 "/api/buttons/press/" + action;
    HTTPClient http;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);

    uint32_t start = millis();
    int code = -1;
    if (http.begin(url)) {
        code = http.POST("");
        http.end();
    }
    Serial.printf("POST %s -> %d (%lu ms)\n", action, code, (unsigned long)(millis() - start));

    if (code >= 200 && code < 300) {
        statusLedFlash(Flash::Ok);
        return;
    }
    statusLedFlash(Flash::Fail);
    // No retry: a repeated start or claps on stage is worse than a miss.
    // A negative code means no connection; the Pi may have a new address.
    if (code < 0) netForgetServerIp();
}

static void senderTask(void*) {
    Gesture gesture;
    for (;;) {
        if (xQueueReceive(queue, &gesture, portMAX_DELAY) == pdTRUE) send(gesture);
    }
}

void senderBegin() {
    queue = xQueueCreate(GESTURE_QUEUE_LEN, sizeof(Gesture));
    xTaskCreate(senderTask, "sender", 8192, nullptr, 1, nullptr);
}

bool senderEnqueue(Gesture gesture) {
    return xQueueSend(queue, &gesture, 0) == pdTRUE;
}
```

- [ ] **Step 3: Wire it into `src/main.cpp`**

Add the include after `#include "ota.h"`:

```cpp
#include "sender.h"
```

In `setup()`, after `netBegin();`:

```cpp
    senderBegin();
```

In `loop()`, replace the gesture block with:

```cpp
    if (gesture != Gesture::None) {
        Serial.printf("Gesture -> %s\n", actionFor(gesture));
        if (!senderEnqueue(gesture)) Serial.println("Drop: queue full");
    }
```

- [ ] **Step 4: Build**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add src/sender.h src/sender.cpp src/main.cpp
git commit -m "Send gestures to StageController from a background task"
```

---

### Task 5: Flash, verify on hardware, README

The board currently runs the old `button-esp32` firmware, with OTA password `flamingods2024`. The first upload goes over OTA to that hostname. Later uploads go to `stage-pillar.local`.

**Every gesture triggers real stage actions** (music, smoke, bubbles). Tell the user before each hardware step and let them perform the presses.

**Files:**
- Create: `README.md`

- [ ] **Step 1: First upload via the old firmware's OTA**

Run: `ESP_OTA_PASSWORD=flamingods2024 pio run -e esp32dev-ota -t upload --upload-port button-esp32.local`
Expected: `Uploading: [====] 100%` and `Done`.

If `button-esp32.local` doesn't resolve, ask the user for the board's IP, or connect it over USB and run `pio run -e esp32dev -t upload`.

- [ ] **Step 2: Confirm the board is up under its new name**

Run: `ping -c 3 stage-pillar.local`
Expected: replies. If this fails, the Wi-Fi credentials in `secrets.h` are wrong; reflash over USB.

- [ ] **Step 3: Verify OTA on the new firmware**

Run: `ESP_OTA_PASSWORD=<OTA_PASSWORD from secrets.h> pio run -e esp32dev-ota -t upload`
Expected: upload succeeds to `stage-pillar.local`.

- [ ] **Step 4: Verify each gesture reaches the server**

Have the user do the gestures in this order: long press, 1, 2, 3 and 4 presses, then a long press to stop the show again. After each one, run:

Run: `curl -s 'http://flamingods.local:8000/api/buttons/recent?limit=1'`
Expected, in order:
1. `stop`
2. `start`
3. `claps`
4. `special`
5. `skip`
6. `stop`

The onboard LED blinks once each time.

- [ ] **Step 5: Verify 5 presses send nothing**

The user presses 5 times.
Run: `curl -s 'http://flamingods.local:8000/api/buttons/recent?limit=1'`
Expected: still the last `stop`; no new event.

- [ ] **Step 6: Verify an unreachable server fails safely** (Review Focus 4)

1. In `include/secrets.h`, temporarily set `SERVER_PORT 8001`, where nothing listens.
2. Upload over OTA (as in Step 3).
3. The user double-presses, then single-presses straight away.

Expected:
- Three fast blinks for each.
- No new event in `/api/buttons/recent`.
- The user feels no delay in the button.

If the user can watch the serial monitor over USB (`pio device monitor`), it shows `POST claps -> -1`, then `Server: ... is ...` (looked up again), then `POST start -> -1`.

Then restore `SERVER_PORT 8000`, upload over OTA again, and repeat one single press. Expected: `start` arrives.

- [ ] **Step 7: Write `README.md`**

````markdown
# StagePillar Button

ESP32 firmware for the stage button. Press gestures send actions to StageController
(`POST http://flamingods.local:8000/api/buttons/press/<action>`).

| Gesture             | Action    |
|---------------------|-----------|
| 1 press             | `start`   |
| 2 presses           | `claps`   |
| 3 presses           | `special` |
| 4 presses           | `skip`    |
| Long press (1.5 s)  | `stop`    |

5+ presses are ignored. A gesture fires 0.4 s after the last release; a long press fires while held.

## Wiring

Momentary switch between **GPIO 4** and **GND** (internal pull-up). Status on the onboard LED (GPIO 2):
one blink = sent, three fast blinks = failed, slow blink = no WiFi.

## Setup

```bash
cp include/secrets.example.h include/secrets.h   # then edit
pio test -e native                                # unit tests on the host
pio run -e esp32dev -t upload                     # USB
ESP_OTA_PASSWORD=... pio run -e esp32dev-ota -t upload   # OTA to stage-pillar.local
```
````

- [ ] **Step 8: Commit**

```bash
git add README.md
git commit -m "Add README with wiring, gestures and flashing"
```
