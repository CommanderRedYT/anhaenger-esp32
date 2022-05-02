#include <Arduino.h>

// system includes
#include <deque>

// 3rdparty lib includes
#include <ESPNowW.h>
#include <FastLED.h>
#include <HardwareSerial.h>
#include <WiFi.h>

#define ESP_RX 16
#define ESP_TX 17

#define LED_COUNT 128
#define LED_PIN 18

struct esp_now_message_t
{
    std::string content;
    std::string type;
};

uint8_t ESP_NOW_MAC[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

std::deque<esp_now_message_t> message_queue{};

std::string buffer;

CRGB leds[LED_COUNT];

void onRecv(const uint8_t* mac_addr, const uint8_t* data, int data_len) {
  const std::string data_str{ data, data + data_len };

  size_t sep_pos = data_str.find(":");
  if (std::string::npos != sep_pos)
  {
      esp_now_message_t msg{
          .content = std::string{data_str.substr(sep_pos+1, data_str.length()-sep_pos-1)},
          .type = std::string{data_str.substr(0, sep_pos)}
      };

      message_queue.push_back(msg);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting...");
  Serial1.begin(115200, SERIAL_8N1, ESP_RX, ESP_TX);

  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAP("bobby_anhaenger", "Passwort_123", 1, 1);

  ESPNow.init();
  ESPNow.add_peer(ESP_NOW_MAC, 1);
  ESPNow.reg_recv_cb(onRecv);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(255);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);

  // serial log ap info
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
}

void loop() {
  if (message_queue.size() > 0)
  {
    for (const esp_now_message_t &msg : message_queue)
    {
      Serial.println(msg.content.c_str());
      Serial1.printf("{\"type\":\"%s\",\"content\":\"%s\"}\r\n", msg.type.c_str(), msg.content.c_str());
    }
  }
  message_queue.erase(std::begin(message_queue), std::end(message_queue));

  while (Serial1.available())
  {
    char c = Serial1.read();
    Serial.print(c);
    Serial1.print(c);
    if (c == '\n' || c == '\r')
    {
      // ESPNow.send_message(ESP_NOW_MAC, (uint8_t*)buffer.c_str(), buffer.length());
      Serial.printf("Sending message: %s\r\n", buffer.c_str());
      buffer.clear();
    }
    else
    {
      buffer += c;
    }
  }
}