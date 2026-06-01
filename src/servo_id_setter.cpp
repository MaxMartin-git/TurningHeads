#include <Arduino.h>
#include <SCServo.h>

SCSCL sc;

namespace {
constexpr uint8_t kServoTxPin = 21;
constexpr uint8_t kServoRxPin = 20;
constexpr uint32_t kServoBaud = 1000000;
constexpr int kIdChangeFrom = 1;
constexpr int kIdChangeTo = 2;
}  // namespace

void logResult(const char* label, int value) {
  Serial.printf("%s: %d (err=%d)\n", label, value, sc.getErr());
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== Servo ID Setter ===");
  Serial.printf("Change %d -> %d\n", kIdChangeFrom, kIdChangeTo);

  Serial1.begin(kServoBaud, SERIAL_8N1, kServoRxPin, kServoTxPin);
  sc.pSerial = &Serial1;
  delay(1000);

  logResult("Ping before (old ID)", sc.Ping(kIdChangeFrom));
  logResult("Ping before (new ID)", sc.Ping(kIdChangeTo));

  int r = sc.unLockEprom(kIdChangeFrom);
  logResult("unLockEprom", r);

  r = sc.writeByte(kIdChangeFrom, SCSCL_ID, kIdChangeTo);
  logResult("writeByte(ID)", r);

  delay(100);
  r = sc.LockEprom(kIdChangeTo);
  logResult("LockEprom", r);

  delay(200);
  logResult("Ping after (old ID)", sc.Ping(kIdChangeFrom));
  logResult("Ping after (new ID)", sc.Ping(kIdChangeTo));

  int idValueOld = sc.readByte(kIdChangeFrom, SCSCL_ID);
  logResult("Read ID register with old ID", idValueOld);

  int idValueNew = sc.readByte(kIdChangeTo, SCSCL_ID);
  logResult("Read ID register with new ID", idValueNew);

  delay(100);
  Serial.println("Done.");
}

void loop() {
}