#include <Arduino.h>
#include <SCServo.h>

SMS_STS st;
int ID_ChangeFrom = 1;
int ID_Changeto = 2;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Serial.println();
  Serial.println("=== Servo ID Setter ===");
  Serial.printf("Change %d -> %d\n", ID_ChangeFrom, ID_Changeto);

  Serial1.begin(1000000, SERIAL_8N1, 20, 21);
  st.pSerial = &Serial1;

  int r;
  r = st.unLockEprom(ID_ChangeFrom);
  Serial.printf("unLockEprom(%d): %s\n", ID_ChangeFrom, r >= 0 ? "OK" : "FAILED");

  r = st.writeByte(ID_ChangeFrom, SMS_STS_ID, ID_Changeto);
  Serial.printf("writeByte(ID): %s\n", r >= 0 ? "OK" : "FAILED");

  delay(100);
  r = st.LockEprom(ID_Changeto);
  Serial.printf("LockEprom(%d): %s\n", ID_Changeto, r >= 0 ? "OK" : "FAILED");

  delay(100);
  r = st.Ping(ID_Changeto);
  Serial.printf("Verification ping(%d): %s\n", ID_Changeto, r >= 0 ? "OK" : "FAILED");
  Serial.println("Done.");
}

void loop() {
}