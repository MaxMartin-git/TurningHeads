#include <Arduino.h>
#include <WiFi.h>

#ifndef TH_NODE_ENABLE_DC_MOTOR
#define TH_NODE_ENABLE_DC_MOTOR 0
#endif

#ifndef TH_NODE_ENABLE_EYEBALL_SERVOS
#define TH_NODE_ENABLE_EYEBALL_SERVOS 0
#endif

#ifndef TH_NODE_ENABLE_SAT_RELAYS
#define TH_NODE_ENABLE_SAT_RELAYS 0
#endif

#ifndef TH_NODE_ENABLE_AS5600
#define TH_NODE_ENABLE_AS5600 0
#endif

#ifndef NODE_ID
#define NODE_ID 1
#endif

#include "shared/th_roles.h"
#include "shared/th_input_modes.h"
#include "shared/th_protocol.h"
#include "node/th_wifi_station.h"

#if TH_NODE_ENABLE_DC_MOTOR
#include "node/th_motor_driver.h"
#endif

#if TH_NODE_ENABLE_SAT_RELAYS
#include "node/th_satellite_relays.h"
#endif

#if TH_NODE_ENABLE_AS5600
#include "node/th_base_angle_sensor.h"
#endif

// ============ KONFIGURATION ============
const char* WIFI_SSID = "ESP_TH";            // Muss gleich wie Coordinator sein
const char* WIFI_PASS = "TurningHeads123";   // Muss gleich wie Coordinator sein
const char* COORDINATOR_IP = "192.168.4.1";  // Standard IP des Coordinators
const uint16_t COORDINATOR_PORT = 5000 + NODE_ID;  // Je Node ein eigener Port

#if TH_NODE_ENABLE_DC_MOTOR
const uint16_t PWM_PIN = 3;                  // PWM fuer Motor an GPIO3
#endif

#if TH_NODE_ENABLE_SAT_RELAYS
const uint16_t RELAY_LIGHT_PIN = 5;          // Relay control pin for satellite light output
const uint16_t RELAY_FOG_PIN = 4;            // Relay control pin for satellite fog output
const bool RELAY_ACTIVE_HIGH = true;
#endif

#if TH_NODE_ENABLE_AS5600
const uint8_t AS5600_I2C_ADDR = 0x36;
const uint8_t AS5600_RAW_ANGLE_REG = 0x0C;
const uint8_t AS5600_SDA_PIN = 8;
const uint8_t AS5600_SCL_PIN = 9;
const uint32_t AS5600_POLL_INTERVAL_MS = 20;
#endif

// ============ GLOBALE VARIABLEN ============
WiFiClient tcpClient;

#if TH_NODE_ENABLE_DC_MOTOR
th::NodeMotorDriver motorDriver(PWM_PIN);
#endif

int16_t currentServoUpdown = 0;
int16_t currentServoLateral = 0;

#if TH_NODE_ENABLE_SAT_RELAYS
th::SatelliteRelays satelliteRelays(RELAY_LIGHT_PIN, RELAY_FOG_PIN, RELAY_ACTIVE_HIGH);
#endif

bool nodeReadySent = false;
uint32_t lastHeartbeatSentMs = 0;
const uint32_t HEARTBEAT_INTERVAL_MS = 1000;
th::InputMode currentInputMode = th::InputMode::Manual;
th::DeviceProfile thisNodeProfile = th::getCurrentNodeProfile();

#if TH_NODE_ENABLE_AS5600
th::BaseAngleSensor baseAngleSensor(
  AS5600_I2C_ADDR,
  AS5600_RAW_ANGLE_REG,
  AS5600_SDA_PIN,
  AS5600_SCL_PIN,
  AS5600_POLL_INTERVAL_MS);
#endif

#if TH_NODE_ENABLE_AS5600
void sendBaseAngleReport(uint16_t rawAngle, int16_t deg10Angle) {
  if (!tcpClient.connected()) {
    return;
  }

  char tcpMsg[24];
  th::buildAngleReportCommand(rawAngle, deg10Angle, tcpMsg, sizeof(tcpMsg));
  tcpClient.print(tcpMsg);
}
#endif

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n\n=== TurningHeads Node %d (%s) ===\n", NODE_ID, thisNodeProfile.label);

#if TH_NODE_ENABLE_DC_MOTOR
  if (thisNodeProfile.capabilities.hasDcMotor) {
    motorDriver.begin();
    Serial.printf("[PWM] Initialized on GPIO%d\n", PWM_PIN);
  } else {
    Serial.println("[PWM] Compiled, but role has no DC motor capability");
  }
#else
  Serial.println("[PWM] Module not compiled for this node env");
#endif

#if TH_NODE_ENABLE_AS5600
  if (thisNodeProfile.capabilities.hasDcMotor) {
    baseAngleSensor.begin();
    Serial.printf("[AS5600] I2C initialized on SDA=%u SCL=%u\n", AS5600_SDA_PIN, AS5600_SCL_PIN);
  } else {
    Serial.println("[AS5600] Compiled, but role has no DC motor capability");
  }
#else
  Serial.println("[AS5600] Module not compiled for this node env");
#endif

#if TH_NODE_ENABLE_SAT_RELAYS
  if (th::isSatelliteRole(thisNodeProfile.role)) {
    satelliteRelays.begin();
    Serial.printf("[LIGHT] Relay output initialized on GPIO%d\n", RELAY_LIGHT_PIN);
    Serial.printf("[FOG] Relay output initialized on GPIO%d\n", RELAY_FOG_PIN);
  } else {
    Serial.println("[RELAY] Compiled, but role is not satellite");
  }
#else
  Serial.println("[RELAY] Module not compiled for this node env");
#endif

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
      tcpClient.setNoDelay(true);
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

#if TH_NODE_ENABLE_AS5600
  if (th::isBaseRole(thisNodeProfile.role) && thisNodeProfile.capabilities.hasDcMotor) {
    uint16_t rawAngle = 0;
    int16_t deg10Angle = 0;
    if (baseAngleSensor.pollChanged(&rawAngle, &deg10Angle)) {
      sendBaseAngleReport(rawAngle, deg10Angle);
    }
  }
#endif

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

#if TH_NODE_ENABLE_DC_MOTOR
      int motorValue = -1;
      int motorDir = motorDriver.dir();
      if (th::tryParseMotorCommand(line, &motorValue, &motorDir)) {
        if (motorValue >= 0 && motorValue <= 255 && motorDir >= 0 && motorDir <= 1) {
          if (th::isBaseRole(thisNodeProfile.role) && thisNodeProfile.capabilities.hasDcMotor) {
            motorDriver.apply(static_cast<uint8_t>(motorValue), static_cast<uint8_t>(motorDir));
            Serial.printf("[MOTOR %s] Set pwm=%d dir=%d\n", th::toString(thisNodeProfile.side), motorDriver.pwm(), motorDriver.dir());
          } else {
            Serial.printf("[MOTOR] Ignored (node role %s)\n", th::toString(thisNodeProfile.role));
          }
        } else {
          Serial.printf("[MOTOR] Invalid payload for node %d\n", NODE_ID);
        }
        continue;
      }
#endif

#if TH_NODE_ENABLE_EYEBALL_SERVOS
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
#endif

#if TH_NODE_ENABLE_SAT_RELAYS
      int lightState = -1;
      if (th::tryParseLightCommand(line, &lightState)) {
        if (th::isSatelliteRole(thisNodeProfile.role)) {
          bool requestedLightOn = (lightState != 0);
          if (requestedLightOn != satelliteRelays.lightOn()) {
            satelliteRelays.applyLight(requestedLightOn);
            Serial.printf("[LIGHT %s] %s%s\n",
                          th::toString(thisNodeProfile.side),
                          satelliteRelays.lightOn() ? "ON" : "OFF",
                          satelliteRelays.lightBreakerActive() ? " (breaker active)" : "");
          }
        } else {
          Serial.printf("[LIGHT] Ignored (node role %s)\n", th::toString(thisNodeProfile.role));
        }
        continue;
      }

      int lightBreakerState = -1;
      if (th::tryParseLightBreakerCommand(line, &lightBreakerState)) {
        if (th::isSatelliteRole(thisNodeProfile.role)) {
          bool requestedBreakerActive = (lightBreakerState != 0);
          if (requestedBreakerActive != satelliteRelays.lightBreakerActive()) {
            satelliteRelays.applyLightBreaker(requestedBreakerActive);
            Serial.printf("[LIGHT BREAKER %s] %s\n",
                          th::toString(thisNodeProfile.side),
                          satelliteRelays.lightBreakerActive() ? "ACTIVE" : "RELEASED");
          }
        } else {
          Serial.printf("[LIGHT BREAKER] Ignored (node role %s)\n", th::toString(thisNodeProfile.role));
        }
        continue;
      }

      int fogState = -1;
      if (th::tryParseFogCommand(line, &fogState)) {
        if (th::isSatelliteRole(thisNodeProfile.role)) {
          bool requestedFogOn = (fogState != 0);
          if (requestedFogOn != satelliteRelays.fogOn()) {
            satelliteRelays.applyFog(requestedFogOn);
            Serial.printf("[FOG %s] %s\n", th::toString(thisNodeProfile.side), satelliteRelays.fogOn() ? "ON" : "OFF");
          }
        } else {
          Serial.printf("[FOG] Ignored (node role %s)\n", th::toString(thisNodeProfile.role));
        }
        continue;
      }
#endif

    }
  }

  delay(2);

  if (!tcpClient.connected()) {
    Serial.println("[TCP] Connection lost, will reconnect...");
    tcpClient.stop();
    nodeReadySent = false;
  }
}
