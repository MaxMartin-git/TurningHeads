#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SCServo.h>

// ============ KONFIGURATION ============
const char* WIFI_SSID = "TurningHeads";
const char* WIFI_PASS = "12345";
const uint16_t TCP_PORT = 5000;
const uint16_t MOTOR_PWM_PIN = 3;  // PWM-Eingang am Motortreiber
const uint16_t MOTOR_DIR_PIN = 4;  // DIR-Eingang am Motortreiber
const uint8_t PWM_CHANNEL = 0;
const uint16_t PWM_FREQ = 1000;  // Hz
const uint8_t PWM_RESOLUTION = 8;  // 8-bit = 0..255
const uint8_t SERVO_UART_TX_PIN = 21;
const uint8_t SERVO_UART_RX_PIN = 20;
const uint32_t SERVO_BAUD = 1000000;  // SC09 braucht 1 Mbps
const uint8_t SERVO_ID = 1;
const uint16_t SERVO_POS_MIN = 0;
const uint16_t SERVO_POS_MAX = 1023;
const uint16_t SERVO_SPEED = 4095;  // Explicit max speed (stable setting)

// ============ GLOBALE VARIABLEN ============
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WiFiServer tcpServer(TCP_PORT);
WiFiClient tcpClient;
uint8_t currentMotorPwm = 0;    // 0..255
uint8_t currentMotorDir = 0;    // 0=CW, 1=CCW
uint8_t currentServoValue = 128; // 0..255 (UI-Wert)
SCSCL scServo;

// ============ HILFSFUNKTION: HTML/CSS/JS WEB-UI ============
const char* getWebPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>TurningHeads Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <style>
    html, body {
      height: 100%;
      overflow: hidden;
      overscroll-behavior: none;
    }
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
    
    <label>Servo Position (0..255)</label>
    <input type="range" id="servoSlider" min="0" max="255" value="128">
    <div id="servoValue">128</div>

    <label>DC Motor PWM (0..255)</label>
    <input type="range" id="motorSlider" min="0" max="255" value="0">
    <div id="motorValue">0</div>

    <label>DC Motor Richtung</label>
    <div>
      <button id="dirCw" type="button">CW</button>
      <button id="dirCcw" type="button">CCW</button>
    </div>

    <div id="status">
      <div>WebSocket: <span id="wsStatus" class="status-error">Disconnected</span></div>
      <div>Node: <span id="nodeStatus" class="status-error">No signal</span></div>
    </div>
  </div>

  <script>
    // === WebSocket Verbindung ===
    const ws = new WebSocket('ws://' + window.location.host + '/ws');
    const servoSlider = document.getElementById('servoSlider');
    const servoValue = document.getElementById('servoValue');
    const motorSlider = document.getElementById('motorSlider');
    const motorValue = document.getElementById('motorValue');
    const dirCw = document.getElementById('dirCw');
    const dirCcw = document.getElementById('dirCcw');
    const wsStatus = document.getElementById('wsStatus');
    const nodeStatus = document.getElementById('nodeStatus');
    let currentDir = 0;

    function sendControlState() {
      if (ws.readyState !== WebSocket.OPEN) {
        return;
      }

      ws.send(JSON.stringify({
        servo: parseInt(servoSlider.value, 10),
        motorPwm: parseInt(motorSlider.value, 10),
        motorDir: currentDir
      }));
    }

    function setDirButtons(dir) {
      currentDir = dir;
      dirCw.style.background = dir === 0 ? '#4CAF50' : '#555';
      dirCcw.style.background = dir === 1 ? '#4CAF50' : '#555';
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
      // Server schickt Status zurück
      if (data.nodeConnected) {
        nodeStatus.textContent = 'Connected';
        nodeStatus.className = 'status-ok';
      } else {
        nodeStatus.textContent = 'Disconnected';
        nodeStatus.className = 'status-error';
      }

      if (typeof data.servo === 'number') {
        servoSlider.value = data.servo;
        servoValue.textContent = data.servo;
      }

      if (typeof data.motorPwm === 'number') {
        motorSlider.value = data.motorPwm;
        motorValue.textContent = data.motorPwm;
      }

      if (typeof data.motorDir === 'number') {
        setDirButtons(data.motorDir);
      }
    };

    servoSlider.addEventListener('input', function() {
      servoValue.textContent = servoSlider.value;
      sendControlState();
    });

    motorSlider.addEventListener('input', function() {
      motorValue.textContent = motorSlider.value;
      sendControlState();
    });

    dirCw.addEventListener('click', function() {
      setDirButtons(0);
      sendControlState();
    });

    dirCcw.addEventListener('click', function() {
      setDirButtons(1);
      sendControlState();
    });

    // Optional: Initial-Status abfragen
    window.addEventListener('load', function() {
      setDirButtons(0);
      if (ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({cmd: 'getStatus'}));
      }
    });

    // Seite gegen Touch-Scroll sperren, Slider-Bewegung aber erlauben
    document.addEventListener('touchmove', function(event) {
      if (!event.target.closest('input[type="range"]')) {
        event.preventDefault();
      }
    }, { passive: false });
  </script>
</body>
</html>
)rawliteral";
}

void applyServoFromUiValue(uint8_t servoValue) {
  // Direkt mapping: 0..255 -> 0..1023
  uint16_t targetPos = (uint16_t)(servoValue * 1023 / 255);
  scServo.WritePos(SERVO_ID, targetPos, 0, SERVO_SPEED);
  currentServoValue = servoValue;
}

void applyDcMotorDriver(uint8_t pwmValue, uint8_t dirValue) {
  currentMotorPwm = pwmValue;
  currentMotorDir = dirValue ? 1 : 0;

  // Wahrheitstabelle:
  // PWM=Low -> Brake (DIR don't care)
  // PWM=High + DIR=Low -> CW
  // PWM=High + DIR=High -> CCW
  if (currentMotorPwm == 0) {
    digitalWrite(MOTOR_DIR_PIN, LOW);
    analogWrite(MOTOR_PWM_PIN, 0);
    return;
  }

  digitalWrite(MOTOR_DIR_PIN, currentMotorDir == 0 ? LOW : HIGH);
  analogWrite(MOTOR_PWM_PIN, currentMotorPwm);
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

      int servoValue = -1;
      if (parseJsonIntInRange(buffer, "servo", 0, 255, &servoValue)) {
        if ((uint8_t)servoValue != currentServoValue) {
          Serial.printf("[SERVO] %u -> %d\n", currentServoValue, servoValue);
          applyServoFromUiValue((uint8_t)servoValue);
        }
      }

      int newMotorPwm = currentMotorPwm;
      int newMotorDir = currentMotorDir;
      bool motorChanged = false;
      int parsedMotorPwm = -1;
      int parsedMotorDir = -1;

      if (parseJsonIntInRange(buffer, "motorPwm", 0, 255, &parsedMotorPwm)) {
        newMotorPwm = parsedMotorPwm;
        motorChanged = true;
      }
      if (parseJsonIntInRange(buffer, "motorDir", 0, 1, &parsedMotorDir)) {
        newMotorDir = parsedMotorDir;
        motorChanged = true;
      }

      if (motorChanged) {
        if ((uint8_t)newMotorPwm != currentMotorPwm || (uint8_t)newMotorDir != currentMotorDir) {
          Serial.printf("[MOTOR] PWM=%d DIR=%d\n", newMotorPwm, newMotorDir);
        }
        applyDcMotorDriver((uint8_t)newMotorPwm, (uint8_t)newMotorDir);

        if (tcpClient.connected()) {
          char tcpMsg[20];
          snprintf(tcpMsg, sizeof(tcpMsg), "M:%d\n", currentMotorPwm);
          tcpClient.print(tcpMsg);
        }
      }

      if (strstr(buffer, "\"getStatus\"") != NULL) {
        char statusMsg[140];
        snprintf(statusMsg, sizeof(statusMsg), "{\"servo\":%u,\"motorPwm\":%u,\"motorDir\":%u,\"nodeConnected\":%s}",
                 currentServoValue, currentMotorPwm, currentMotorDir,
                 tcpClient.connected() ? "true" : "false");
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

  // Motortreiber-Eingänge initialisieren
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  applyDcMotorDriver(0, 0);
  Serial.printf("[MOTOR] PWM GPIO%d, DIR GPIO%d\n", MOTOR_PWM_PIN, MOTOR_DIR_PIN);

  // UART-Servo (SC09) initialisieren: TX=GPIO21, RX=GPIO20
  Serial1.begin(SERVO_BAUD, SERIAL_8N1, SERVO_UART_RX_PIN, SERVO_UART_TX_PIN);
  scServo.pSerial = &Serial1;
  delay(100);
  applyServoFromUiValue(currentServoValue);

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
  if (millis() - lastServoUpdate >= 50) {  // Alle 50ms: weniger UART-Last, gleiches Fahrgefühl
    applyServoFromUiValue(currentServoValue);
    lastServoUpdate = millis();
  }

  delay(10);
}

