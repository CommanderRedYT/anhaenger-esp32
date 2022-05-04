// use \r\n for newline in Serial1 messages
#include <Arduino.h>

// system includes
#include <deque>

// 3rdparty lib includes
#include <ArduinoJson.h>
#include <ESPNowW.h>
#include <FastLED.h>
#include <HardwareSerial.h>
#include <WiFi.h>

// local includes
#include "led.h"
#include "ledmagic.h"

// animations
#include "DefaultAnimation.h"
#include "RainbowAnimation.h"

struct esp_now_message_t
{
    std::string content;
    std::string type;
    uint16_t id;
};

uint8_t ESP_NOW_MAC[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint16_t anhaenger_id = 0;

bool pauseAnimation{false};

led_state_t led_state;

std::deque<esp_now_message_t> message_queue{};
std::string buffer;

CRGB leds[LED_COUNT];
led_magic_wrapper<LED_COUNT, RING_COUNT, leds> led_magic;

const animation_info_t animations[] = {
  { DefaultAnimation, "Default"},
  { RainbowAnimation, "Rainbow" },
};

uint8_t current_animation = 0;

uint8_t getAnimationByName(const std::string& name)
{
    for (uint8_t i = 0; i < sizeof(animations) / sizeof(animations[0]); ++i) {
        if (name == animations[i].name) {
            return i;
        }
    }
    return -1;
}

void onRecv(const uint8_t* mac_addr, const uint8_t* data, int data_len) {
  const std::string data_str{ data, data + data_len };

  size_t sep_pos = data_str.find(":");
  if (std::string::npos != sep_pos)
  {
      const std::string type = std::string{data_str.substr(0, sep_pos)};
      std::string content = std::string{data_str.substr(sep_pos+1, data_str.length()-sep_pos-1)};
      uint16_t id = 0;

      size_t sep_pos2 = content.find(":");
      if (std::string::npos != sep_pos2)
      {
        id = atoi(content.substr(0, sep_pos2).c_str());
        content = content.substr(sep_pos2+1, content.length()-sep_pos2-1);
      }
      else
      {
        Serial.printf("could not find : in \"%s\"\n", data_str.c_str());
      }

      esp_now_message_t msg {
        .content = content,
        .type = type,
        .id = id
      };

      if (msg.id == anhaenger_id)
      {
        message_queue.push_back(msg);
      }
  }
}

void setup() {

  Serial.begin(115200);
  Serial.println("Booting...");

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);

  FastLED.setBrightness(255);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 300);
  led_magic.fill(CRGB::Black);
  FastLED.show();
  Serial.println("LEDMagic done.");

  Serial1.begin(115200, SERIAL_8N1, ESP_RX, ESP_TX);
  Serial1.println("Serial done.");

  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAP("bobby_anhaenger", "Passwort_123", 1, 1);
  Serial.println("WiFi done.");

  ESPNow.init();
  ESPNow.add_peer(ESP_NOW_MAC, 1);
  ESPNow.reg_recv_cb(onRecv);
  Serial.println("ESPNow done.");

  // serial log ap info
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial1.println("{\"type\":\"boot_info\",\"content\":\"" + WiFi.softAPIP().toString() + "\"}");
  led_magic.fill(CRGB::Black);
  FastLED.show();

  Serial.println("Boot done.");
}

bool special_event_occurred()
{
  return (led_state.blink_left || led_state.blink_right || led_state.brake);
}

void handleLeds() {
  const uint32_t now = millis();

  if (pauseAnimation)
  {
    return;
  }

  // check if current_animation is in bounds, if not, set to 0
  if (current_animation >= sizeof(animations) / sizeof(animations[0]))
  {
    current_animation = 0;
  }

  if (special_event_occurred())
  {
    led_magic.fill(CRGB::Black);

    if (led_state.blink_left)
    {
      led_magic.fillRing((now % 750 < 375) ? CRGB::Yellow : CRGB::Black, 0);
    }
    if (led_state.blink_right)
    {
      led_magic.fillRing((now % 750 < 375) ? CRGB::Yellow : CRGB::Black, 1);
    }
    if (led_state.brake)
    {
      led_magic.fill(CRGB::Red);
    }
  }
  else
  {
    animations[current_animation].animation();
  }
}

void loop() {
  const uint32_t now = millis();
  EVERY_N_SECONDS(1)
  {
      Serial1.printf("{\"type\":\"heartbeat\",\"content\":\"%u\"}\r\n", now);
  }

  if (message_queue.size() > 0)
  {
    for (const esp_now_message_t &msg : message_queue)
    {
      // Serial.printf("{\"type\":\"%s\",\"content\":\"%s\",\"id\":\"%u\"}\n", msg.type.c_str(), msg.content.c_str(), msg.id);
      Serial1.printf("{\"type\":\"%s\",\"content\":\"%s\",\"id\":\"%u\"}\r\n", msg.type.c_str(), msg.content.c_str(), msg.id);
      if (msg.type == "BLINKLEFT")
      {
        led_state.blink_left = 1250;
      }
      else if (msg.type == "BLINKRIGHT")
      {
        led_state.blink_right = 1250;
      }
      else if (msg.type == "BLINKBOTH")
      {
        led_state.blink_left = 1250;
        led_state.blink_right = 1250;
      }
      else if (msg.type == "BLINKOFF")
      {
        led_state.blink_left = 0;
        led_state.blink_right = 0;
      }
      else if (msg.type == "BRAKELIGHTSON")
      {
        led_state.brake = 1250;
      }
      else if (msg.type == "BRAKELIGHTSOFF")
      {
        led_state.brake = 0;
      }
    }
  }
  message_queue.erase(std::begin(message_queue), std::end(message_queue));

  while (Serial1.available())
  {
    char c = Serial1.read();
    Serial1.print(c);
    if (c == '\n' || c == '\r')
    {
      StaticJsonDocument<128> doc;
      const auto err = deserializeJson(doc, buffer);
      if (err)
      {
        Serial.printf("Error: %s\n", err.c_str());
        buffer.clear();
        continue;
      }

      const std::string type = doc["type"];
      const std::string content = doc["content"];
    
      Serial.printf("Type: %s, Content: %s, ID: %hu\n", type.c_str(), content.c_str(), anhaenger_id);

      if (type == "led_test")
      {
        pauseAnimation = (content == "true");
        led_magic.fill(pauseAnimation ? CRGB::Red : CRGB::Black, 0, 17);
      }
      else if (type == "set_id")
      {
        anhaenger_id = atoi(content.c_str());
      }
      else if (type == "brightness")
      {
        const auto brightness = atoi(content.c_str());
        FastLED.setBrightness(brightness);
      }
      else if (type == "animation")
      {
        const auto animation = getAnimationByName(content);
        if (animation != -1)
        {
          current_animation = animation;
        }
      }
      
      buffer.clear();
    }
    else
    {
      buffer += c;
    }
  }

  EVERY_N_MILLIS(1) {
    if (led_state.blink_left)
      led_state.blink_left--;
    if (led_state.blink_right)
      led_state.blink_right--;
    if (led_state.brake)
      led_state.brake--;
  }
  
  handleLeds();

  EVERY_N_MILLIS(50) // render every 50ms
  {
    FastLED.show();
  }
}