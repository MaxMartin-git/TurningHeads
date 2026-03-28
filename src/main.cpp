#include <Arduino.h>
#include <SCServo.h>

// ESP32-C3-Zero default UART0 pins exposed as TX=GPIO21, RX=GPIO20.
constexpr int kServoTxPin = 21;
constexpr int kServoRxPin = 20;
constexpr uint8_t kServoId = 1;

// SC09 range is 0..1023 for ~300 degrees.
constexpr uint16_t kPosA = 200;
constexpr uint16_t kPosB = 800;
constexpr uint16_t kSpeed = 1500;
constexpr uint32_t kHalfPeriodMs = 500;

SCSCL sc;

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial1.begin(1000000, SERIAL_8N1, kServoRxPin, kServoTxPin);
  sc.pSerial = &Serial1;

  Serial.println("SC09 UART test started");
  Serial.println("Using ID=1, UART=1Mbps, TX=GPIO21, RX=GPIO20");
}

void loop() {
  int id = sc.Ping(kServoId);
  if (id == -1) {
    Serial.println("Ping failed: check power, ID, UART pins, and baud");
    delay(1000);
    return;
  }

  sc.WritePos(kServoId, kPosA, 0, kSpeed);
  Serial.println("Move A");
  delay(kHalfPeriodMs);

  sc.WritePos(kServoId, kPosB, 0, kSpeed);
  Serial.println("Move B");
  delay(kHalfPeriodMs);
}
