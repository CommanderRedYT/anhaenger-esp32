// use \r\n for newline in Serial1 messages
#include <Arduino.h>

// system includes
#include <deque>

// 3rdparty lib includes
#include <ArduinoJson.h>
#include <ArduinoNvs.h>
#include <ESPNowW.h>
#include <FastLED.h>
#include <HardwareSerial.h>
#include <WiFi.h>

// local includes
#include "led.h"
#include "ledmagic.h"
#include "mapping.h"

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
bool boot_info_sent{false};

led_state_t led_state;

std::deque<esp_now_message_t> message_queue{};
std::string buffer;

CRGB leds[LED_COUNT];
led_magic_wrapper<LED_COUNT, RING_COUNT, leds> led_magic;
led_ring_mapping_t led_ring_mapping[RING_COUNT];

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

std::string getAnimationName(uint8_t index)
{
    return animations[index].name;
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

      esp_now_message_t msg {
        .content = std::string::npos != sep_pos2 ? content.substr(0, sep_pos2) : content,
        .type = type,
        .id = std::string::npos != sep_pos2 ? id : uint16_t{0},
      };

      if (msg.id == anhaenger_id || msg.id == 0)
      {
        message_queue.push_back(msg);
      }
  }
  else
  {
    Serial.printf("could not find 1st \":\" in \"%s\"\n", data_str.c_str());
  }
}

void store_mapping(uint8_t front_left, uint8_t front_right, uint8_t back_left, uint8_t back_right, bool store_in_nvs = true)
{
  led_ring_mapping[0] = led_ring_mapping_t{ .ring_id = front_left, .direction = led_ring_direction_t::front_left };
  led_ring_mapping[1] = led_ring_mapping_t{ .ring_id = front_right, .direction = led_ring_direction_t::front_right };
  led_ring_mapping[2] = led_ring_mapping_t{ .ring_id = back_left, .direction = led_ring_direction_t::back_left };
  led_ring_mapping[3] = led_ring_mapping_t{ .ring_id = back_right, .direction = led_ring_direction_t::back_right };

  if (!store_in_nvs)
    return;

  NVS.setInt("front_left", front_left);
  NVS.setInt("front_right", front_right);
  NVS.setInt("back_left", back_left);
  NVS.setInt("back_right", back_right);

  NVS.commit();
}

void load_mapping()
{
  uint8_t front_left = NVS.getInt("front_left");
  uint8_t front_right = NVS.getInt("front_right");
  uint8_t back_left = NVS.getInt("back_left");
  uint8_t back_right = NVS.getInt("back_right");

  // check if at least two rings have the same value
  const uint8_t sum = front_left + front_right + back_left + back_right;
  if (sum != 6)
  {
    Serial.println("mapping is invalid, using default");
    store_mapping(0, 1, 2, 3);
    return;
  }

  Serial.println("mapping successfully loaded");

  store_mapping(front_left, front_right, back_left, back_right, false);
}

void save_mapping()
{
  store_mapping(led_ring_mapping[0].ring_id, led_ring_mapping[1].ring_id, led_ring_mapping[2].ring_id, led_ring_mapping[3].ring_id);
}

void setup() {

  Serial.begin(115200);
  Serial.println("Booting...");

  // NVS
  NVS.begin();
  load_mapping();

  // FastLED
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(255);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 300);
  led_magic.fill(CRGB::Black);
  FastLED.show();
  Serial.println("LEDMagic done.");

  // Serial
  Serial1.begin(115200, SERIAL_8N1, ESP_RX, ESP_TX);
  Serial.println("Serial done.");

  // WiFi AP for ESP-Now
  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAP("bobby_anhaenger", "Passwort_123", 1, 1);
  Serial.println("WiFi done.");

  // ESP-Now
  ESPNow.init();
  ESPNow.add_peer(ESP_NOW_MAC, 1);
  ESPNow.reg_recv_cb(onRecv);
  Serial.println("ESPNow done.");

  // Log AP Info
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  led_magic.fill(CRGB::Black);
  FastLED.show();

  Serial.println("Boot done.");

  boot_info_sent = false;
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

void send_boot_info()
{
  StaticJsonDocument<256> doc;
  std::string json;

  for (uint8_t i = 0; i < sizeof(animations) / sizeof(animations[0]); i++)
  {
    doc.add(animations[i].name);
  }

  serializeJson(doc, json);

  // Serial.printf("{\"type\":\"boot_info\",\"content\": { \"ap_ip\":\"%s\", \"hostname\":\"%s\", \"ap_name\":\"%s\", \"animations\": %s } }\r\n", WiFi.softAPIP().toString().c_str(), WiFi.getHostname(), WiFi.softAPSSID().c_str(), json.c_str());
  Serial1.printf("{\"type\":\"boot_info\",\"content\": { \"ap_ip\":\"%s\", \"hostname\":\"%s\", \"ap_name\":\"%s\", \"animations\": %s } }\r\n", WiFi.softAPIP().toString().c_str(), WiFi.getHostname(), WiFi.softAPSSID().c_str(), json.c_str());
}

void send_config()
{
  send_boot_info();
  std::string json;
  StaticJsonDocument<256> doc;
  doc["id"] = anhaenger_id;
  doc["brightness"] = FastLED.getBrightness();
  doc["animation"] = getAnimationName(current_animation);

  JsonArray mapping = doc.createNestedArray("ring_mapping");

  for (uint8_t i = 0; i < RING_COUNT; i++)
  {
    mapping.add(led_ring_mapping[i].ring_id);
  }

  serializeJson(doc, json);
  Serial1.printf("{\"type\":\"config\",\"content\":%s}\r\n", json.c_str());
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
      // Serial.printf("{\"type\":\"%s\",\"content\":\"%s\",\"id\":\"%u\"}\n"  , msg.type.c_str(), msg.content.c_str(), msg.id);
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
        Serial.printf("Error: %s (%s)\n", err.c_str(), buffer.c_str());
        Serial1.printf("{\"type\":\"error\",\"content\":\"%s\"}\r\n", err.c_str());
        buffer.clear();
        continue;
      }

      const std::string type = doc["type"];
      const std::string content = doc["content"];

      if (type == "led_test")
      {
        pauseAnimation = (content == "true");
        if (pauseAnimation)
        {
          led_magic.fill(CRGB::Black);
          led_magic.fillRing(CRGB::White, 0, 0, 1);
          led_magic.fillRing(CRGB::White, 1, 0, 2);
          led_magic.fillRing(CRGB::White, 2, 0, 3);
          led_magic.fillRing(CRGB::White, 3, 0, 4);
        }
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
      else if (type == "update_mapping")
      {
        auto front_left = doc["front_left"].as<uint8_t>();
        auto front_right = doc["front_right"].as<uint8_t>();
        auto back_left = doc["back_left"].as<uint8_t>();
        auto back_right = doc["back_right"].as<uint8_t>();

        store_mapping(front_left, front_right, back_left, back_right);
      }
      else if (type == "restart")
      {
        ESP.restart();
      }
      else if (type == "boot_info_response")
      {
        boot_info_sent = true;
      }
      else if (type == "getConfig")
      {
        send_config();
      }
      else if (type == "setConfig")
      {
        const std::string key = doc["k"];
        const auto _value = doc["v"];

        if (_value.is<bool>())
        {
          const bool value = _value;
          Serial.printf("Set config %s to %s (bool)\n", key.c_str(), value ? "true" : "false");
        }
        else if (_value.is<int>())
        {
          const int value = _value;

          if (key == "ring_mapping")
          {
            const uint8_t ring_id = value;
            const uint8_t ring_index = doc["i"].as<uint8_t>();
            Serial.printf("Set mapping[%d] to %d\n", ring_index, ring_id);

            // update and save
            led_ring_mapping[ring_index].ring_id = ring_id;
            save_mapping();
          }
          else
          {
            Serial.printf("Set config %s to %d (int)\n", key.c_str(), value);
            if (key == "brightness")
            {
              FastLED.setBrightness(value);
            }
            else if (key == "id")
            {
              anhaenger_id = value;
            }
          }
        }
        else if (_value.is<float>())
        {
          const float value = _value;
          Serial.printf("Set config %s to %f (float)\n", key.c_str(), value);
        }
        else if (_value.is<std::string>())
        {
          const std::string value = _value;
          Serial.printf("Set config %s to %s (string)\n", key.c_str(), value.c_str());
          if (key == "animation")
          {
            const auto animation = getAnimationByName(value);
            if (animation != -1)
            {
              current_animation = animation;
            }
          }
        }
        else
        {
          Serial.printf("Set config %s to %s (unknown)\n", key.c_str(), _value.as<std::string>().c_str());
        }

        send_config();
      }
      else
      {
        Serial.printf("Type: %s, Content: %s, ID: %hu\n", type.c_str(), content.c_str(), anhaenger_id);
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

  if (!boot_info_sent)
  {
    EVERY_N_SECONDS(1) {
      Serial.println("Sending boot info");
      send_boot_info();
    }
  }
}
