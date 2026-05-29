#include <Arduino.h>
#include <SCServo.h>

SMS_STS st;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  delay(500);
  Serial.println();
  Serial.println("=== Servo ID Reader ===");
  Serial1.begin(1000000, SERIAL_8N1, 20, 21);
  st.pSerial = &Serial1;
  bool found = false;

  for (int id = 1; id <= 2; id++) {
    if (st.Ping(id) >= 0) {
      Serial.printf("Servo ID: %d\n", id);
      found = true;
      break;
    }
  }

  if (!found) {
    Serial.println("No servo found.");
  }
}

void loop() {
}