#pragma once

#include <Arduino.h>

namespace th {

inline bool tryParseMotorCommand(const String& line, int* motorValue, int* motorDir) {
  if (!line.startsWith("M:")) {
    return false;
  }

  int firstSep = line.indexOf(':', 2);
  if (firstSep < 0) {
    *motorValue = line.substring(2).toInt();
    if (motorDir != nullptr) {
      *motorDir = 0;
    }
    return true;
  }

  String pwmToken = line.substring(2, firstSep);
  String dirToken = line.substring(firstSep + 1);

  *motorValue = pwmToken.toInt();
  if (motorDir != nullptr) {
    *motorDir = dirToken.toInt();
  }
  return true;
}

inline bool tryParseServoCommand(const String& line, int* updownValue, int* lateralValue) {
  if (!line.startsWith("S:")) {
    return false;
  }

  int firstSep = line.indexOf(':', 2);
  if (firstSep < 0) {
    return false;
  }

  String upToken = line.substring(2, firstSep);
  String latToken = line.substring(firstSep + 1);
  *updownValue = upToken.toInt();
  *lateralValue = latToken.toInt();
  return true;
}

inline void buildMotorCommand(uint8_t pwmValue, uint8_t dirValue, char* buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "M:%u:%u\n", pwmValue, dirValue ? 1 : 0);
}

inline void buildServoCommand(int16_t updownValue, int16_t lateralValue, char* buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "S:%d:%d\n", updownValue, lateralValue);
}

}  // namespace th
