#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace th {

inline void startAccessPoint(const char* ssid, const char* pass) {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, pass);
  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("[WiFi] AP started: SSID='%s', Pass='%s', IP=%s\\n", ssid, pass, apIP.toString().c_str());
}

}  // namespace th
