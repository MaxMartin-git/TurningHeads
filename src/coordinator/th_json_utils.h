#pragma once

#include <Arduino.h>
#include <cstring>

namespace th {

inline bool parseJsonIntInRange(const char* buffer, const char* key, int minVal, int maxVal, int* outValue) {
  char keyPattern[32];
  snprintf(keyPattern, sizeof(keyPattern), "\"%s\"", key);

  char* keyPos = strstr(buffer, keyPattern);
  if (keyPos == nullptr) {
    return false;
  }

  char* colon = strchr(keyPos, ':');
  if (colon == nullptr) {
    return false;
  }

  int value = atoi(colon + 1);
  if (value < minVal || value > maxVal) {
    return false;
  }

  *outValue = value;
  return true;
}

inline bool parseJsonBool(const char* buffer, const char* key, bool* outValue) {
  char keyPattern[32];
  snprintf(keyPattern, sizeof(keyPattern), "\"%s\"", key);

  char* keyPos = strstr(buffer, keyPattern);
  if (keyPos == nullptr) {
    return false;
  }

  char* colon = strchr(keyPos, ':');
  if (colon == nullptr) {
    return false;
  }

  char* valueStart = colon + 1;
  while (*valueStart == ' ' || *valueStart == '\t') {
    ++valueStart;
  }

  if (strncmp(valueStart, "true", 4) == 0 || *valueStart == '1') {
    *outValue = true;
    return true;
  }

  if (strncmp(valueStart, "false", 5) == 0 || *valueStart == '0') {
    *outValue = false;
    return true;
  }

  return false;
}

}  // namespace th
