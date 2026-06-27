#pragma once

#include <Arduino.h>

namespace th {

enum class InputMode : uint8_t {
  Manual = 0,
  Sequence = 1,
};

struct ControlState {
  int16_t servoUpdown;
  int16_t servoLateral;
  uint8_t motorPwm;
  uint8_t motorDir;
};

struct SequenceSelection {
  uint16_t sequenceId;
  uint16_t stepIndex;
};

struct ControlEnvelope {
  InputMode mode;
  ControlState state;
  SequenceSelection sequence;
};

inline ControlEnvelope makeManualEnvelope(const ControlState& state) {
  ControlEnvelope envelope;
  envelope.mode = InputMode::Manual;
  envelope.state = state;
  envelope.sequence = {0, 0};
  return envelope;
}

}  // namespace th
