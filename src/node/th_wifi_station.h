#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace th {

inline void connectStationWifi(const char* ssid, const char* pass, uint8_t maxAttempts) {
  Serial.printf("[WiFi] Connecting to SSID '%s'...\\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, pass);

  uint8_t attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\\n[WiFi] Connected! IP: %s\\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\\n[WiFi] Failed to connect, will retry in loop");
  }
}

inline void keepStationWifiAlive() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }
}

}  // namespace th
