#include <Arduino.h>
#include <WiFi.h>

#ifndef NODE_ID
#define NODE_ID 1
#endif

// ============ KONFIGURATION ============
const char* WIFI_SSID = "ESP_TH";            // Muss gleich wie Coordinator sein
const char* WIFI_PASS = "TurningHeads123";   // Muss gleich wie Coordinator sein
const char* COORDINATOR_IP = "192.168.4.1";  // Standard IP des Coordinators
const uint16_t COORDINATOR_PORT = 5000 + NODE_ID;  // Je Node ein eigener Port
const uint16_t PWM_PIN = 3;                  // PWM fuer Motor an GPIO3

// ============ GLOBALE VARIABLEN ============
WiFiClient tcpClient;
uint8_t currentMotorValue = 0;
bool nodeReadySent = false;
uint32_t lastHeartbeatSentMs = 0;
const uint32_t HEARTBEAT_INTERVAL_MS = 1000;

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n\n=== TurningHeads Node %d ===\n", NODE_ID);

  // PWM fuer Motor initialisieren
  pinMode(PWM_PIN, OUTPUT);
  analogWrite(PWM_PIN, 0);
  Serial.printf("[PWM] Initialized on GPIO%d\n", PWM_PIN);

  // Verbindung zum Coordinator-WLAN
  Serial.printf("[WiFi] Connecting to SSID '%s'...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] Failed to connect, will retry in loop");
  }
}

// ============ LOOP ============
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }

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

      // Parse: "M:123" (Motor-Wert 0..255)
      if (line.startsWith("M:")) {
        int motorValue = line.substring(2).toInt();
        if (motorValue >= 0 && motorValue <= 255) {
          currentMotorValue = motorValue;
          analogWrite(PWM_PIN, currentMotorValue);
          Serial.printf("[MOTOR] Set to %d\n", currentMotorValue);
        }
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
