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

## Wiring

Momentary switch between **GPIO 12** and **GND** (internal pull-up). Status on the onboard LED (GPIO 2):
one blink = sent, three fast blinks = failed, slow blink = no WiFi.

## Setup

```bash
cp include/secrets.example.h include/secrets.h   # then edit
pio test -e native                                # unit tests on the host
pio run -e esp32dev -t upload                     # USB
ESP_OTA_PASSWORD=... pio run -e esp32dev-ota -t upload   # OTA to stage-pillar.local
```
