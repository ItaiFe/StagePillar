# StagePillar Button

ESP32 firmware for the stage button. Press gestures send actions to StageController
(`POST http://flamingods.local:8000/api/buttons/press/<action>`).

| Gesture             | Action    |
|---------------------|-----------|
| 1 press             | `start`   |
| 2 presses           | `claps`   |
| 3 presses           | `special` |
| 4 presses           | `skip`    |
| 5+ presses          | `special` |
| Long press (1.5 s)  | `stop`    |

A gesture fires 0.4 s after the last release; a long press fires while held.

While no song is loaded (stopped, or the Pi unreachable; paused counts as running) only `start` (single press) and `stop`
(long press) do anything; other gestures are ignored, with no request and no LED effect. The button polls
`GET /api/player/state` every second.

## Wiring

Button on **GPIO 12** (internal pull-up), reads HIGH while pressed. Status on the onboard LED (GPIO 2):
one blink = sent, three fast blinks = failed, slow blink = no WiFi.

## Pillar LEDs

100 × WS2812B on **GPIO 4** (GRB), brightness capped at 80/255. Built-in effects live in
`lib/pillar/led_catalogue.cpp`; default sequences per slot in `lib/pillar/pillar_player.cpp`.

| Slot      | Default                                                              |
|-----------|----------------------------------------------------------------------|
| idle      | Slow rainbow flowing up (loops while no show is running)             |
| start     | Bouncing comet, magenta → cyan → gold, one colour per pass (loops for the whole song) |
| `claps`   | White sparkles                                                       |
| `special` | Pulses blue → purple → magenta                                       |
| `skip`    | Cyan band runs up                                                    |
| `stop`    | Fades red to a dim glow                                              |

A gesture plays its slot once, then returns to the play loop while the show runs, or to idle.
When the show ends (stop pressed or the song ends) the stop fade plays, then idle. Two quick red
flashes mean the press did not reach the server. The strip is dark during OTA.

## Setup

```bash
cp include/secrets.example.h include/secrets.h   # then edit
pio test -e native                                # unit tests on the host
pio run -e esp32dev -t upload                     # USB
ESP_OTA_PASSWORD=... pio run -e esp32dev-ota -t upload   # OTA to stage-pillar.local
```
