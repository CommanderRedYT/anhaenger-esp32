#pragma once

// 3rdparty lib includes
#include <Arduino.h>
#include <FastLED.h>

// local includes
#include "led.h"

void DefaultAnimation() {
    EVERY_N_SECONDS(1) {
        led_magic.fill(CRGB::Red, 0, LED_COUNT / RING_COUNT / 2);
        led_magic.fill(CRGB::White, LED_COUNT / RING_COUNT / 2, LED_COUNT / RING_COUNT);
    }
}