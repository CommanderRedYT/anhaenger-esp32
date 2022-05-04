#pragma once

// 3rdparty lib includes
#include <Arduino.h>
#include <FastLED.h>

// local includes
#include "led.h"

void RainbowAnimation() {
    const uint32_t now = millis();
    static uint16_t index = 0;

    EVERY_N_MILLIS(20)
    {
        if (index++ > 255) {
            index = 0;
        }
        led_magic.fill_rainbow(index, 8);
    }
}