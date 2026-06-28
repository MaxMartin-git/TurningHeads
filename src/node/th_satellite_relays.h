#pragma once

#include <Arduino.h>

namespace th {

class SatelliteRelays {
 public:
  SatelliteRelays(uint8_t lightPin, uint8_t fogPin, bool activeHigh)
      : lightPin_(lightPin),
        fogPin_(fogPin),
        activeHigh_(activeHigh),
        lightOn_(false),
        lightBreakerActive_(false),
        fogOn_(false) {}

  void begin() {
    pinMode(lightPin_, OUTPUT);
    pinMode(fogPin_, OUTPUT);
    applyLight(false);
    applyLightBreaker(false);
    applyFog(false);
  }

  void applyLight(bool lightOn) {
    lightOn_ = lightOn;
    bool effectiveLightOn = lightOn_ && !lightBreakerActive_;
    digitalWrite(lightPin_, effectiveLightOn ? onLevel() : offLevel());
  }

  void applyLightBreaker(bool breakerActive) {
    lightBreakerActive_ = breakerActive;
    // Re-apply light output with breaker override.
    applyLight(lightOn_);
  }

  void applyFog(bool fogOn) {
    fogOn_ = fogOn;
    digitalWrite(fogPin_, fogOn_ ? onLevel() : offLevel());
  }

  bool lightOn() const { return lightOn_; }
  bool lightBreakerActive() const { return lightBreakerActive_; }
  bool fogOn() const { return fogOn_; }

 private:
  int onLevel() const { return activeHigh_ ? HIGH : LOW; }
  int offLevel() const { return activeHigh_ ? LOW : HIGH; }

  uint8_t lightPin_;
  uint8_t fogPin_;
  bool activeHigh_;
  bool lightOn_;
  bool lightBreakerActive_;
  bool fogOn_;
};

}  // namespace th
