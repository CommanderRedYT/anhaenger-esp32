#pragma once

// 3rdparty lib includes
#include <Arduino.h>
#include <FastLED.h>

template<size_t NUM_LEDS, size_t NUM_RINGS, CRGB(&LEDS)[NUM_LEDS]>
struct led_magic_wrapper {
    struct proxy {
        size_t index;

        operator CRGB&() { return LEDS[index]; }

        proxy& operator=(const CRGB& rhs)
        {
            for (size_t i = 0; i < NUM_RINGS; ++i) {
                LEDS[index + (i * (NUM_LEDS / NUM_RINGS))] = rhs;
            }
            return *this;
        }
    };

    struct iterator {
        size_t index;

        iterator& operator++() { ++index; return *this; }
        iterator& operator--() { --index; return *this; }

        proxy operator*() { return {index}; }

        bool operator==(const iterator& rhs) { return index == rhs.index; }
        bool operator!=(const iterator& rhs) { return index != rhs.index; }

        ptrdiff_t operator-(const iterator& rhs) { return index - rhs.index; }
    };

    iterator begin() { return { 0 }; }
    iterator end() { return { NUM_LEDS / NUM_RINGS }; }

    proxy operator[](size_t index) { return { index }; }

    void fill(const CRGB& color)
    {
        for (size_t i = 0; i < NUM_LEDS; ++i) {
            LEDS[i] = color;
        }
    }

    void fillRing(const CRGB& color, size_t ring, size_t start, size_t end)
    {
        end = std::min(end, NUM_LEDS / NUM_RINGS);
        for (size_t i = start; i < end; ++i) {
            LEDS[i + (ring * (NUM_LEDS / NUM_RINGS))] = color;
        }
    }

    void fillRing(const CRGB& color, size_t ring)
    {
        fillRing(color, ring, 0, NUM_LEDS / NUM_RINGS);
    }

    void fill(const CRGB& color, size_t start, size_t end)
    {
        end = std::min(end, NUM_LEDS);
        for (size_t i = start; i < end; ++i) {
            (*this)[i] = color;
        }
    }

    void every2nd(const CRGB& color)
    {
        for (size_t i = 0; i < NUM_LEDS; i++) {
            if (i % 2 == 0) {
                LEDS[i] = color;
            }
        }
    }

    void everyOther2nd(const CRGB& color)
    {
        for (size_t i = 0; i < NUM_LEDS; i++) {
            if (i % 2 == 1) {
                LEDS[i] = color;
            }
        }
    }

    void fill_rainbow(uint8_t initialhue, uint8_t deltahue)
    {
        CHSV hsv;
        hsv.hue = initialhue;
        hsv.val = 255;
        hsv.sat = 240;
        for( int i = 0; i < NUM_LEDS / NUM_RINGS; ++i) {
            CRGB temp;
            hsv2rgb_rainbow(hsv, temp);
            (*this)[i] = temp;
            hsv.hue += deltahue;
        }
    }
};