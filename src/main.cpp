#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

constexpr uint8_t kRgbLedPin = 10;
constexpr uint8_t kRgbLedCount = 1;

Adafruit_NeoPixel pixels(kRgbLedCount, kRgbLedPin, NEO_GRB + NEO_KHZ800);

void setColor(uint8_t r, uint8_t g, uint8_t b) {
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  pixels.begin();
  pixels.clear();
  pixels.setBrightness(32);
  pixels.show();
  Serial.println("ESP32-C3-Zero WS2812 test started (GPIO10)");
}

void loop() {
  setColor(255, 0, 0);
  Serial.println("RGB RED");
  delay(500);

  setColor(0, 255, 0);
  Serial.println("RGB GREEN");
  delay(500);

  setColor(0, 0, 255);
  Serial.println("RGB BLUE");
  delay(500);

  setColor(0, 0, 0);
  Serial.println("RGB OFF");
  delay(500);
}
