#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SCServo.h>

// ============ KONFIGURATION ============
const char* WIFI_SSID = "TurningHeads";
const char* WIFI_PASS = "12345";
const uint16_t TCP_PORT = 5000;
const uint16_t PWM_PIN = 3;  // PWM für Motor an GPIO3
const uint8_t PWM_CHANNEL = 0;
const uint16_t PWM_FREQ = 1000;  // Hz
const uint8_t PWM_RESOLUTION = 8;  // 8-bit = 0..255
const uint8_t SERVO_UART_TX_PIN = 21;
const uint8_t SERVO_UART_RX_PIN = 20;
const uint32_t SERVO_BAUD = 1000000;  // SC09 braucht 1 Mbps, nicht 38400!
const uint8_t SERVO_ID = 1;
const uint16_t SERVO_POS_MIN = 0;
const uint16_t SERVO_POS_MAX = 1023;
const uint16_t SERVO_SPEED = 4095;  // Maximum speed for SC09

// ============ GLOBALE VARIABLEN ============
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WiFiServer tcpServer(TCP_PORT);
WiFiClient tcpClient;
uint8_t currentMotorValue = 0;  // Aktueller PWM-Wert für Motor (0..255)
uint8_t currentServoAngle = 90; // Aktueller Servo-Winkel (0..180)
SCSCL scServo;

// ============ HILFSFUNKTION: HTML/CSS/JS WEB-UI ============
const char* getWebPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>TurningHeads Control</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      max-width: 600px;
      margin: 50px auto;
      background: #222;
      color: #fff;
      text-align: center;
    }
    h1 { 
      color: #4CAF50; 
      margin-bottom: 30px;
    }
    .container {
      background: #333;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 0 10px rgba(0,0,0,0.5);
    }
    label {
      display: block;
      margin: 20px 0 10px;
      font-weight: bold;
    }
    input[type="range"] {
      width: 100%;
      height: 10px;
      border-radius: 5px;
      background: #555;
      outline: none;
      -webkit-appearance: none;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: #4CAF50;
      cursor: pointer;
    }
    input[type="range"]::-moz-range-thumb {
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: #4CAF50;
      cursor: pointer;
      border: none;
    }
    #value {
      display: inline-block;
      background: #4CAF50;
      padding: 10px 20px;
      border-radius: 5px;
      margin-top: 10px;
      font-weight: bold;
      min-width: 60px;
    }
    #status {
      margin-top: 20px;
      padding: 10px;
      border-radius: 5px;
      background: #444;
      font-size: 14px;
    }
    .status-ok { color: #4CAF50; }
    .status-error { color: #f44336; }
  </style>
</head>
<body>
  <div class="container">
    <h1>🎮 TurningHeads</h1>
    
    <label>Motor Speed</label>
    <input type="range" id="motorSlider" min="0" max="255" value="0">
    <div id="value">0</div>

    <div id="status">
      <div>WebSocket: <span id="wsStatus" class="status-error">Disconnected</span></div>
      <div>Node: <span id="nodeStatus" class="status-error">No signal</span></div>
    </div>
  </div>

  <script>
    // === WebSocket Verbindung ===
    const ws = new WebSocket('ws://' + window.location.host + '/ws');
    const slider = document.getElementById('motorSlider');
    const valueDisplay = document.getElementById('value');
    const wsStatus = document.getElementById('wsStatus');
    const nodeStatus = document.getElementById('nodeStatus');

    function sendMotorState(value) {
      if (ws.readyState !== WebSocket.OPEN) {
        return;
      }

      ws.send(JSON.stringify({
        motor: value
      }));
    }

    // WebSocket verbunden
    ws.onopen = function(event) {
      console.log('WebSocket connected');
      wsStatus.textContent = 'Connected';
      wsStatus.className = 'status-ok';
    };

    // WebSocket Fehler
    ws.onerror = function(event) {
      console.error('WebSocket error');
      wsStatus.textContent = 'Error';
      wsStatus.className = 'status-error';
    };

    // WebSocket Daten empfangen (Status vom Coordinator)
    ws.onmessage = function(event) {
      const data = JSON.parse(event.data);
      // Server schickt Status zurück, z.B. {"motor": 123, "nodeConnected": true}
      if (data.nodeConnected) {
        nodeStatus.textContent = 'Connected';
        nodeStatus.className = 'status-ok';
      } else {
        nodeStatus.textContent = 'Disconnected';
        nodeStatus.className = 'status-error';
      }

      if (typeof data.motor === 'number') {
        slider.value = data.motor;
        valueDisplay.textContent = data.motor;
      }
    };

    // Slider-Event: Live Motor-Wert senden
    slider.addEventListener('input', function() {
      const motorValue = parseInt(slider.value, 10);
      valueDisplay.textContent = motorValue;
      sendMotorState(motorValue);
    });

    // Optional: Initial-Status abfragen
    window.addEventListener('load', function() {
      if (ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({cmd: 'getStatus'}));
      }
    });
  </script>
</body>
</html>
)rawliteral";
}

void applyServoFromMotorValue(uint8_t motorValue) {
  // Direkt mapping: 0..255 -> 0..1023
  uint16_t targetPos = (uint16_t)(motorValue * 1023 / 255);
  scServo.WritePos(SERVO_ID, targetPos, 0, SERVO_SPEED);
  currentServoAngle = motorValue;  // Vereinfacht: direkt kopieren
  // Debug-Ausgabe entfernt (war alle 50ms!)
}

bool parseJsonIntInRange(const char* buffer, const char* key, int minVal, int maxVal, int* outValue) {
  char keyPattern[32];
  snprintf(keyPattern, sizeof(keyPattern), "\"%s\"", key);

  char *keyPos = strstr(buffer, keyPattern);
  if (keyPos == NULL) {
    return false;
  }

  char *colon = strchr(keyPos, ':');
  if (colon == NULL) {
    return false;
  }

  int value = atoi(colon + 1);
  if (value < minVal || value > maxVal) {
    return false;
  }

  *outValue = value;
  return true;
}

// ============ WEBSOCKET EVENT HANDLER ============
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client %u connected\n", client->id());
  } 
  else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client %u disconnected\n", client->id());
  } 
  else if (type == WS_EVT_DATA) {
    // Daten vom Smartphone empfangen (z.B. Motor-Wert)
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->opcode == WS_TEXT) {
      char buffer[len + 1];
      memcpy(buffer, data, len);
      buffer[len] = '\0';
      
      Serial.printf("[WS] Received: %s\n", buffer);

      int motorValue = -1;
      if (parseJsonIntInRange(buffer, "motor", 0, 255, &motorValue)) {
        if ((uint8_t)motorValue != currentMotorValue) {  // Nur bei Änderung ausgeben
          Serial.printf("[MOTOR] %d -> %d\n", currentMotorValue, motorValue);
        }
        currentMotorValue = (uint8_t)motorValue;
        analogWrite(PWM_PIN, currentMotorValue);
        applyServoFromMotorValue(currentMotorValue);

        if (tcpClient.connected()) {
          char tcpMsg[20];
          snprintf(tcpMsg, sizeof(tcpMsg), "M:%d\n", currentMotorValue);
          tcpClient.print(tcpMsg);
        }
      }

      if (strstr(buffer, "\"getStatus\"") != NULL) {
        char statusMsg[96];
        snprintf(statusMsg, sizeof(statusMsg), "{\"motor\":%u,\"nodeConnected\":%s}",
                 currentMotorValue, tcpClient.connected() ? "true" : "false");
        client->text(statusMsg);
      }
    }
  }
}

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=== TurningHeads Coordinator ===");

  // PWM für Motor initialisieren
  pinMode(PWM_PIN, OUTPUT);
  analogWrite(PWM_PIN, 0);
  Serial.printf("[PWM] Initialized on GPIO%d\n", PWM_PIN);

  // UART-Servo (SC09) initialisieren: TX=GPIO21, RX=GPIO20
  Serial1.begin(SERVO_BAUD, SERIAL_8N1, SERVO_UART_RX_PIN, SERVO_UART_TX_PIN);
  scServo.pSerial = &Serial1;
  delay(100);
  applyServoFromMotorValue(currentMotorValue);

  // WiFi Access Point starten
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("[WiFi] AP started: SSID='%s', Pass='%s', IP=%s\n", 
                WIFI_SSID, WIFI_PASS, apIP.toString().c_str());

  // WebSocket initialisieren
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  // HTTP Endpoints
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", getWebPage());
  });

  // Server starten
  server.begin();
  Serial.println("[HTTP] Web server started on port 80");

  // TCP Server für Node starten
  tcpServer.begin();
  Serial.printf("[TCP] Listening on port %d for Node\n", TCP_PORT);
}

// ============ LOOP ============
void loop() {
  // Auf neue TCP-Verbindung vom Node prüfen
  if (!tcpClient.connected()) {
    WiFiClient newClient = tcpServer.available();
    if (newClient) {
      tcpClient = newClient;
      Serial.println("[TCP] Node connected!");
    }
  } 
  else {
    // Optional: Nachrichten vom Node lesen (falls Node zurücksendet)
    while (tcpClient.available()) {
      String line = tcpClient.readStringUntil('\n');
      Serial.printf("[TCP RX] %s\n", line.c_str());
    }
  }

  // Kontinuierliche Servo-Ansteuerung (periodisches Refresh)
  static unsigned long lastServoUpdate = 0;
  if (millis() - lastServoUpdate >= 50) {  // Alle 50ms
    applyServoFromMotorValue(currentMotorValue);
    lastServoUpdate = millis();
  }

  delay(10);
}

