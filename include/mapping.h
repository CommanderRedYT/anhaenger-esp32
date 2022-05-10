#pragma once

// system includes
#include <Arduino.h>

enum class led_ring_direction_t : uint8_t {
    front_left = 0,
    front_right = 1,
    back_left = 2,
    back_right = 3,
};

std::string led_ring_direction_to_string(led_ring_direction_t direction)
{
    switch (direction) {
    case led_ring_direction_t::front_left:
        return "front_left";
    case led_ring_direction_t::front_right:
        return "front_right";
    case led_ring_direction_t::back_left:
        return "back_left";
    case led_ring_direction_t::back_right:
        return "back_right";
    default:
        return "unknown";
    }
}

led_ring_direction_t string_to_led_ring_direction(const std::string& str)
{
    if (str == "front_left") {
        return led_ring_direction_t::front_left;
    } else if (str == "front_right") {
        return led_ring_direction_t::front_right;
    } else if (str == "back_left") {
        return led_ring_direction_t::back_left;
    } else if (str == "back_right") {
        return led_ring_direction_t::back_right;
    } else {
        return led_ring_direction_t::front_left;
    }
}

typedef struct {
    uint8_t ring_id;
    led_ring_direction_t direction;
} led_ring_mapping_t;

extern led_ring_mapping_t led_ring_mapping[RING_COUNT];