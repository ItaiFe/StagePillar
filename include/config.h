#pragma once
#include <stdint.h>

#if !__has_include("secrets.h")
#error "Missing include/secrets.h: copy include/secrets.example.h to include/secrets.h and fill it in"
#endif
#include "secrets.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

constexpr uint8_t BUTTON_PIN = 12;  // switch to GND, internal pull-up (GPIO 4 drives the LED strip)
constexpr uint8_t LED_PIN = 2;      // onboard LED, active high
constexpr const char* DEVICE_HOSTNAME = "stage-pillar";
constexpr uint16_t HTTP_TIMEOUT_MS = 2000;
constexpr uint32_t MDNS_TIMEOUT_MS = 1500;
constexpr uint8_t GESTURE_QUEUE_LEN = 4;
