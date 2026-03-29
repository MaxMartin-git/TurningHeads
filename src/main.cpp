#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

// ============ KONFIGURATION ============
const char* WIFI_SSID = "TurningHeads";
const char* WIFI_PASS = "12345";
const uint16_t TCP_PORT = 5000;
const uint16_t PWM_PIN = 3;  // PWM für Motor an GPIO3
const uint8_t PWM_CHANNEL = 0;
const uint16_t PWM_FREQ = 1000;  // Hz
const uint8_t PWM_RESOLUTION = 8;  // 8-bit = 0..255

// ============ GLOBALE VARIABLEN ============
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WiFiServer tcpServer(TCP_PORT);
WiFiClient tcpClient;
uint8_t currentMotorValue = 0;  // Aktueller PWM-Wert für Motor (0..255)

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
    };

    // Slider-Event: Live Slider-Wert senden
    slider.addEventListener('input', function() {
      const value = parseInt(slider.value);
      valueDisplay.textContent = value;
      
      // Sende JSON zum Coordinator übers WebSocket
      const payload = JSON.stringify({
        motor: value
      });
      
      if (ws.readyState === WebSocket.OPEN) {
        ws.send(payload);
      }
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
      
      // Parse JSON: {"motor": 123}
      // Einfache Anfrage: suche "motor": 
      char *motorStr = strstr(buffer, "\"motor\"");
      if (motorStr != NULL) {
        char *colon = strchr(motorStr, ':');
        int motorValue = (colon != nullptr) ? atoi(colon + 1) : -1;
        if (motorValue >= 0 && motorValue <= 255) {
          currentMotorValue = motorValue;
          analogWrite(PWM_PIN, currentMotorValue);
          Serial.printf("[MOTOR] Set to %d\n", currentMotorValue);
          
          // Weiterleiten an Node über TCP
          if (tcpClient.connected()) {
            char tcpMsg[20];
            snprintf(tcpMsg, sizeof(tcpMsg), "M:%d\n", currentMotorValue);
            tcpClient.print(tcpMsg);
          } else {
            Serial.println("[TCP] Node not connected");
          }
        }
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

  delay(10);
}

