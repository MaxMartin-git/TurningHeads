#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=== TurningHeads TEST ===");
}

void loop() {
  Serial.println("[TEST] Hello from ESP32-C3 - still alive!");
  delay(1000);
}
