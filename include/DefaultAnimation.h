#pragma once

// 3rdparty lib includes
#include <Arduino.h>
#include <FastLED.h>

// local includes
#include "led.h"

void DefaultAnimation() {
    EVERY_N_SECONDS(1) {
        led_magic.fillRing(CRGB::Red, 0);
        led_magic.fillRing(CRGB::Red, 1);
        led_magic.fillRing(CRGB::White, 2);
        led_magic.fillRing(CRGB::White, 3);
    }
}