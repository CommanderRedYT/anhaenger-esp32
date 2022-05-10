#pragma once

// system includes
#include <Arduino.h>

// 3rdparty lib includes
#include <FastLED.h>

// local includes
#include "ledmagic.h"

extern CRGB leds[LED_COUNT];
extern led_magic_wrapper<LED_COUNT, RING_COUNT, leds> led_magic;
extern bool pauseAnimation;

struct led_state_t {
    uint32_t brake;
    uint32_t blink_left;
    uint32_t blink_right;
};

extern led_state_t led_state;

typedef void (*animation_t)();
typedef struct {
    animation_t animation;
    std::string name;
} animation_info_t;

extern const animation_info_t animations[];