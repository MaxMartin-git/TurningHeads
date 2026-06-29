#pragma once

#include <Arduino.h>
#include <SCServo.h>

namespace th {

class SatelliteServoController {
 public:
  SatelliteServoController(uint8_t txPin,
                          uint8_t rxPin,
                          uint32_t baud,
                          uint8_t updownId,
                          uint8_t lateralId,
                          uint16_t posMin,
                          uint16_t posMax,
                          uint16_t posCenter,
                          int16_t updownLimit,
                          int16_t lateralLimit,
                          uint16_t speed)
      : txPin_(txPin),
        rxPin_(rxPin),
        baud_(baud),
        updownId_(updownId),
        lateralId_(lateralId),
        posMin_(posMin),
        posMax_(posMax),
        posCenter_(posCenter),
        updownLimit_(updownLimit),
        lateralLimit_(lateralLimit),
        speed_(speed),
        desiredUpdown_(0),
        desiredLateral_(0),
        currentUpdown_(0),
        currentLateral_(0),
        taskStarted_(false),
        mux_(portMUX_INITIALIZER_UNLOCKED) {}

  void begin() {
    Serial1.begin(baud_, SERIAL_8N1, rxPin_, txPin_);
    scServo_.pSerial = &Serial1;
    delay(100);

    // Send initial neutral values via the same path used for runtime updates.
    setTargets(0, 0);

    if (!taskStarted_) {
      xTaskCreatePinnedToCore(taskEntry, "satServoTask", 4096, this, 1, nullptr, 0);
      taskStarted_ = true;
    }
  }

  void setTargets(int16_t updown, int16_t lateral) {
    int16_t processedUpdown = preprocessUpdown(updown);
    int16_t processedLateral = preprocessLateral(lateral);

    currentUpdown_ = processedUpdown;
    currentLateral_ = processedLateral;

    portENTER_CRITICAL(&mux_);
    desiredUpdown_ = processedUpdown;
    desiredLateral_ = processedLateral;
    portEXIT_CRITICAL(&mux_);
  }

  int16_t currentUpdown() const {
    return currentUpdown_;
  }

  int16_t currentLateral() const {
    return currentLateral_;
  }

 private:
  static void taskEntry(void* context) {
    auto* self = static_cast<SatelliteServoController*>(context);
    self->taskLoop();
  }

  void taskLoop() {
    int16_t lastSentUpdown = 0x7FFF;
    int16_t lastSentLateral = 0x7FFF;

    for (;;) {
      int16_t wantUpdown = 0;
      int16_t wantLateral = 0;

      portENTER_CRITICAL(&mux_);
      wantUpdown = desiredUpdown_;
      wantLateral = desiredLateral_;
      portEXIT_CRITICAL(&mux_);

      if (wantUpdown != lastSentUpdown) {
        uint16_t targetPos = offsetToPos(wantUpdown, updownLimit_);
        scServo_.WritePos(updownId_, targetPos, 0, speed_);
        Serial.printf("[SERVO UPDOWN] command=%u bits\n", targetPos);
        lastSentUpdown = wantUpdown;
      }

      if (wantLateral != lastSentLateral) {
        uint16_t targetPos = offsetToPos(wantLateral, lateralLimit_);
        scServo_.WritePos(lateralId_, targetPos, 0, speed_);
        Serial.printf("[SERVO LATERAL] command=%u bits\n", targetPos);
        lastSentLateral = wantLateral;
      }

      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }

  int16_t preprocessUpdown(int16_t rawValue) const {
    // Hook for future filtering/algorithms (currently passthrough + clamp).
    return clampOffset(rawValue, updownLimit_);
  }

  int16_t preprocessLateral(int16_t rawValue) const {
    // Hook for future filtering/algorithms (currently passthrough + clamp).
    return clampOffset(rawValue, lateralLimit_);
  }

  int16_t clampOffset(int16_t value, int16_t limit) const {
    return constrain(value, -limit, limit);
  }

  uint16_t offsetToPos(int16_t offsetValue, int16_t limit) const {
    int16_t clamped = clampOffset(offsetValue, limit);
    int32_t targetPos = static_cast<int32_t>(posCenter_) + clamped;
    return static_cast<uint16_t>(constrain(targetPos, static_cast<int32_t>(posMin_), static_cast<int32_t>(posMax_)));
  }

  uint8_t txPin_;
  uint8_t rxPin_;
  uint32_t baud_;
  uint8_t updownId_;
  uint8_t lateralId_;
  uint16_t posMin_;
  uint16_t posMax_;
  uint16_t posCenter_;
  int16_t updownLimit_;
  int16_t lateralLimit_;
  uint16_t speed_;

  volatile int16_t desiredUpdown_;
  volatile int16_t desiredLateral_;
  int16_t currentUpdown_;
  int16_t currentLateral_;
  bool taskStarted_;
  portMUX_TYPE mux_;
  SCSCL scServo_;
};

}  // namespace th
