#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace th {

class BaseAngleSensor {
 public:
  BaseAngleSensor(uint8_t i2cAddr, uint8_t rawAngleReg, uint8_t sdaPin, uint8_t sclPin, uint32_t pollIntervalMs)
      : i2cAddr_(i2cAddr),
        rawAngleReg_(rawAngleReg),
        sdaPin_(sdaPin),
        sclPin_(sclPin),
        pollIntervalMs_(pollIntervalMs),
        lastPollMs_(0),
        currentRaw_(-1),
        currentDeg10_(-1) {}

  void begin() {
    Wire.begin(sdaPin_, sclPin_);
  }

  bool pollChanged(uint16_t* outRaw, int16_t* outDeg10) {
    if ((millis() - lastPollMs_) < pollIntervalMs_) {
      return false;
    }
    lastPollMs_ = millis();

    uint16_t raw = 0;
    if (!readRawAngle(&raw)) {
      return false;
    }

    int16_t deg10 = rawToDeg10(raw);
    if ((int)raw == currentRaw_ && (int)deg10 == currentDeg10_) {
      return false;
    }

    currentRaw_ = (int)raw;
    currentDeg10_ = (int)deg10;
    *outRaw = raw;
    *outDeg10 = deg10;
    return true;
  }

 private:
  static int16_t rawToDeg10(uint16_t rawAngle) {
    uint32_t scaled = ((uint32_t)rawAngle * 3600UL + 2048UL) / 4096UL;
    if (scaled >= 3600UL) {
      scaled = 0;
    }
    return static_cast<int16_t>(scaled);
  }

  bool readRawAngle(uint16_t* outRawAngle) {
    Wire.beginTransmission(i2cAddr_);
    Wire.write(rawAngleReg_);
    if (Wire.endTransmission(false) != 0) {
      return false;
    }

    uint8_t bytes = Wire.requestFrom((int)i2cAddr_, 2);
    if (bytes != 2) {
      return false;
    }

    uint8_t highByte = Wire.read();
    uint8_t lowByte = Wire.read();
    *outRawAngle = static_cast<uint16_t>(((highByte & 0x0F) << 8) | lowByte);
    return true;
  }

  uint8_t i2cAddr_;
  uint8_t rawAngleReg_;
  uint8_t sdaPin_;
  uint8_t sclPin_;
  uint32_t pollIntervalMs_;
  uint32_t lastPollMs_;
  int currentRaw_;
  int currentDeg10_;
};

}  // namespace th
