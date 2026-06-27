#include <Arduino.h>
#include <WiFi.h>

#ifndef NODE_ID
#define NODE_ID 1
#endif

#include "shared/th_roles.h"
#include "shared/th_input_modes.h"
#include "shared/th_protocol.h"
#include "node/th_wifi_station.h"

// ============ KONFIGURATION ============
const char* WIFI_SSID = "ESP_TH";            // Muss gleich wie Coordinator sein
const char* WIFI_PASS = "TurningHeads123";   // Muss gleich wie Coordinator sein
const char* COORDINATOR_IP = "192.168.4.1";  // Standard IP des Coordinators
const uint16_t COORDINATOR_PORT = 5000 + NODE_ID;  // Je Node ein eigener Port
const uint16_t PWM_PIN = 3;                  // PWM fuer Motor an GPIO3

// ============ GLOBALE VARIABLEN ============
WiFiClient tcpClient;
uint8_t currentMotorValue = 0;
uint8_t currentMotorDir = 0;
int16_t currentServoUpdown = 0;
int16_t currentServoLateral = 0;
bool nodeReadySent = false;
uint32_t lastHeartbeatSentMs = 0;
const uint32_t HEARTBEAT_INTERVAL_MS = 1000;
th::InputMode currentInputMode = th::InputMode::Manual;
th::DeviceProfile thisNodeProfile = th::getCurrentNodeProfile();

void applyDcMotor(uint8_t pwmValue, uint8_t dirValue) {
  currentMotorValue = pwmValue;
  currentMotorDir = dirValue ? 1 : 0;
  analogWrite(PWM_PIN, currentMotorValue);
}

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n\n=== TurningHeads Node %d (%s) ===\n", NODE_ID, thisNodeProfile.label);

  if (thisNodeProfile.capabilities.hasDcMotor) {
    pinMode(PWM_PIN, OUTPUT);
    applyDcMotor(0, 0);
    Serial.printf("[PWM] Initialized on GPIO%d\n", PWM_PIN);
  } else {
    Serial.println("[PWM] Skipped (no DC motor capability)");
  }

  // Verbindung zum Coordinator-WLAN
  th::connectStationWifi(WIFI_SSID, WIFI_PASS, 20);
}

// ============ LOOP ============
void loop() {
  th::keepStationWifiAlive();

  // Wenn nicht verbunden, versuche zu verbinden
  if (!tcpClient.connected()) {
    Serial.printf("[TCP] Attempting to connect to %s:%d\n",
                  COORDINATOR_IP, COORDINATOR_PORT);

    if (tcpClient.connect(COORDINATOR_IP, COORDINATOR_PORT)) {
      Serial.println("[TCP] Connected to Coordinator!");
      nodeReadySent = false;
    } else {
      Serial.println("[TCP] Connection failed, retrying in 2 seconds...");
      delay(2000);
      return;
    }
  }

  if (tcpClient.connected() && !nodeReadySent) {
    tcpClient.printf("NODE_READY %d\n", NODE_ID);
    nodeReadySent = true;
    Serial.printf("[NODE %d] Ready signal sent\n", NODE_ID);
  }

  if (tcpClient.connected() && (millis() - lastHeartbeatSentMs) >= HEARTBEAT_INTERVAL_MS) {
    tcpClient.printf("HEARTBEAT %d\n", NODE_ID);
    lastHeartbeatSentMs = millis();
  }

  // Daten vom Coordinator empfangen und verarbeiten
  while (tcpClient.available()) {
    String line = tcpClient.readStringUntil('\n');
    line.trim();

    if (line.length() > 0) {
      Serial.printf("[TCP RX] %s\n", line.c_str());

      if (line.startsWith("TEST") || line == "PING") {
        tcpClient.printf("PONG %d\n", NODE_ID);
        Serial.printf("[TEST] PONG sent from node %d\n", NODE_ID);
        continue;
      }

      if (line.startsWith("MODE:")) {
        String modeToken = line.substring(5);
        modeToken.trim();
        if (modeToken == "SEQUENCE") {
          currentInputMode = th::InputMode::Sequence;
        } else {
          currentInputMode = th::InputMode::Manual;
        }
        Serial.printf("[MODE] %s\n", currentInputMode == th::InputMode::Manual ? "MANUAL" : "SEQUENCE");
        continue;
      }

      int motorValue = -1;
      int motorDir = currentMotorDir;
      if (th::tryParseMotorCommand(line, &motorValue, &motorDir)) {
        if (motorValue >= 0 && motorValue <= 255 && motorDir >= 0 && motorDir <= 1) {
          if (th::isBaseRole(thisNodeProfile.role) && thisNodeProfile.capabilities.hasDcMotor) {
            applyDcMotor(static_cast<uint8_t>(motorValue), static_cast<uint8_t>(motorDir));
            Serial.printf("[MOTOR %s] Set pwm=%d dir=%d\n", th::toString(thisNodeProfile.side), currentMotorValue, currentMotorDir);
          } else {
            Serial.printf("[MOTOR] Ignored (node role %s)\n", th::toString(thisNodeProfile.role));
          }
        } else {
          Serial.printf("[MOTOR] Invalid payload for node %d\n", NODE_ID);
        }
        continue;
      }

      int servoUpdown = 0;
      int servoLateral = 0;
      if (th::tryParseServoCommand(line, &servoUpdown, &servoLateral)) {
        if (th::isSatelliteRole(thisNodeProfile.role) && thisNodeProfile.capabilities.hasEyeballServos) {
          currentServoUpdown = static_cast<int16_t>(servoUpdown);
          currentServoLateral = static_cast<int16_t>(servoLateral);
          Serial.printf("[SERVO %s] Target updown=%d lateral=%d (TODO: map to SCServo write)\n",
                        th::toString(thisNodeProfile.side),
                        currentServoUpdown,
                        currentServoLateral);
        } else {
          Serial.printf("[SERVO] Ignored (node role %s)\n", th::toString(thisNodeProfile.role));
        }
        continue;
      }

    }
  }

  delay(10);

  if (!tcpClient.connected()) {
    Serial.println("[TCP] Connection lost, will reconnect...");
    tcpClient.stop();
    nodeReadySent = false;
  }
}
