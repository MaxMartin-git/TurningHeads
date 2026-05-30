#include <Arduino.h>

namespace {
constexpr uint8_t kServoTxPin = 21;
constexpr uint8_t kServoRxPin = 20;
constexpr uint32_t kServoBaud = 1000000;
constexpr uint8_t kServoMinId = 1;
constexpr uint8_t kServoMaxId = 253;

constexpr uint8_t kPacketHeader1 = 0xFA;
constexpr uint8_t kPacketHeader2 = 0xAF;
constexpr uint8_t kStoreCenterCommand = 0x0A;
constexpr uint8_t kStoreCenterParameter = 0x00;

uint8_t calculateChecksum(uint8_t id, uint8_t command, uint8_t parameter) {
  const uint16_t sum = static_cast<uint16_t>(id) + command + parameter;
  return static_cast<uint8_t>(~sum);
}

void sendStoreCenterCommand(uint8_t servoId) {
  const uint8_t packet[] = {
      kPacketHeader1,
      kPacketHeader2,
      servoId,
      kStoreCenterCommand,
      kStoreCenterParameter,
      calculateChecksum(servoId, kStoreCenterCommand, kStoreCenterParameter),
  };

  Serial1.write(packet, sizeof(packet));
  Serial1.flush();
}
}  // namespace

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial1.begin(kServoBaud, SERIAL_8N1, kServoRxPin, kServoTxPin);

  Serial.println();
  Serial.println("=== Servo Center Setter ===");
  Serial.println("Move the servo by hand into the desired center position before powering on.");
  Serial.println("The script will try all servo IDs and store the current position for the one that is connected.");

  delay(1500);
  for (int servoId = kServoMinId; servoId <= kServoMaxId; ++servoId) {
    sendStoreCenterCommand(static_cast<uint8_t>(servoId));
  }

  Serial.printf("Sent center-store command across IDs %u..%u.\n", kServoMinId, kServoMaxId);
  Serial.println("Done. Power off if you want to adjust the servo again.");
}

void loop() {
  delay(20);
}