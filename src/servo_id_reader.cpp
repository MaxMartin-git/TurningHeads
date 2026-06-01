#include <Arduino.h>
#include <SCServo.h>

SMS_STS st;

namespace {
constexpr uint8_t kServoTxPin = 21;
constexpr uint8_t kServoRxPin = 20;
constexpr uint32_t kServoBaud = 1000000;
constexpr uint8_t kServoMinId = 1;
constexpr uint8_t kServoMaxId = 253;
}  // namespace

void setup() {
  Serial.begin(115200);

  Serial1.begin(kServoBaud, SERIAL_8N1, kServoRxPin, kServoTxPin);
  st.pSerial = &Serial1;

  Serial.println();
  Serial.println("=== Servo ID Reader ===");
  Serial.println("Starting scan in 2 seconds...");
  delay(2000);
  Serial.printf("Scanning IDs %u..%u at %lu baud on RX=%u TX=%u\n",
                kServoMinId,
                kServoMaxId,
                static_cast<unsigned long>(kServoBaud),
                kServoRxPin,
                kServoTxPin);

  bool found = false;

  for (int id = kServoMinId; id <= kServoMaxId; id++) {
    if (st.Ping(id) >= 0) {
      Serial.printf("Servo ID: %d\n", id);
      found = true;
      break;
    }
  }

  if (!found) {
    Serial.printf("No servo found in ID range %u..%u.\n", kServoMinId, kServoMaxId);
  }
}

void loop() {
}