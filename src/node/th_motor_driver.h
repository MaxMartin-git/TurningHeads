#pragma once

#include <Arduino.h>

namespace th {

class NodeMotorDriver {
 public:
  explicit NodeMotorDriver(uint8_t pwmPin)
      : pwmPin_(pwmPin), currentPwm_(0), currentDir_(0) {}

  void begin() {
    pinMode(pwmPin_, OUTPUT);
    apply(0, 0);
  }

  void apply(uint8_t pwmValue, uint8_t dirValue) {
    currentPwm_ = pwmValue;
    currentDir_ = dirValue ? 1 : 0;
    analogWrite(pwmPin_, currentPwm_);
  }

  uint8_t pwm() const { return currentPwm_; }
  uint8_t dir() const { return currentDir_; }

 private:
  uint8_t pwmPin_;
  uint8_t currentPwm_;
  uint8_t currentDir_;
};

}  // namespace th
