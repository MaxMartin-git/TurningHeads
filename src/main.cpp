#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SCServo.h>

// ============ KONFIGURATION ============
const char* WIFI_SSID = "ESP_TH";
const char* WIFI_PASS = "TurningHeads123";
const uint16_t TCP_PORT = 5000;
const uint16_t MOTOR_PWM_PIN = 3;  // PWM-Eingang am Motortreiber
const uint16_t MOTOR_DIR_PIN = 4;  // DIR-Eingang am Motortreiber
const uint8_t PWM_CHANNEL = 0;
const uint16_t PWM_FREQ = 1000;  // Hz
const uint8_t PWM_RESOLUTION = 8;  // 8-bit = 0..255
const uint8_t SERVO_UART_TX_PIN = 21;
const uint8_t SERVO_UART_RX_PIN = 20;
const uint32_t SERVO_BAUD = 1000000;  // SC09 braucht 1 Mbps
const uint8_t SERVO_UPDOWN_ID = 1;
const uint8_t SERVO_LATERAL_ID = 2;
const uint16_t SERVO_POS_MIN = 0;
const uint16_t SERVO_POS_MAX = 1023;
const uint16_t SERVO_POS_CENTER = 511;
int16_t updown_pos_limit = 140;
int16_t lateral_pos_limit = 80;
const uint16_t SERVO_SPEED = 4095;  // Explicit max speed (stable setting)

// ============ GLOBALE VARIABLEN ============
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WiFiServer tcpServer(TCP_PORT);
WiFiClient tcpClient;
uint8_t currentMotorPwm = 0;    // 0..255
uint8_t currentMotorDir = 0;    // 0=CW, 1=CCW
int16_t currentServoUpdownValue = 0;    // -80..80, 0 = Mitte (511 bits)
int16_t currentServoLateralValue = 0;  // -80..80, 0 = Mitte (511 bits)
SCSCL scServo;
// Desired servo positions written by WebSocket handler; actual writes happen in servoTask
volatile int16_t desiredServoUpdownValue = 0;
volatile int16_t desiredServoLateralValue = 0;
portMUX_TYPE servoMux = portMUX_INITIALIZER_UNLOCKED;

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
    
    <label>Servo Updown (-140..140)</label>
    <input type="range" id="servoUpdownSlider" min="-140" max="140" value="0">
    <div id="servoUpdownValue">0</div>

    <label>Servo Lateral (-80..80)</label>
    <input type="range" id="servoLateralSlider" min="-80" max="80" value="0">
    <div id="servoLateralValue">0</div>

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
    const servoUpdownSlider = document.getElementById('servoUpdownSlider');
    const servoUpdownValue = document.getElementById('servoUpdownValue');
    const servoLateralSlider = document.getElementById('servoLateralSlider');
    const servoLateralValue = document.getElementById('servoLateralValue');
    const motorSlider = document.getElementById('motorSlider');
    const motorValue = document.getElementById('motorValue');
    const dirCw = document.getElementById('dirCw');
    const dirCcw = document.getElementById('dirCcw');
    const wsStatus = document.getElementById('wsStatus');
    const nodeStatus = document.getElementById('nodeStatus');
    let currentDir = 0;
    let controlSendTimer = null;
    let statusSynced = false;
    let controlStateDirty = false;
    const CONTROL_DEBOUNCE_MS = 80;

    function buildControlState() {
      return {
        servoUpdown: parseInt(servoUpdownSlider.value, 10),
        servoLateral: parseInt(servoLateralSlider.value, 10),
        motorPwm: parseInt(motorSlider.value, 10),
        motorDir: currentDir
      };
    }

    function sendControlState() {
      if (ws.readyState !== WebSocket.OPEN) {
        return;
      }

      ws.send(JSON.stringify(buildControlState()));
    }

    function scheduleControlStateSend() {
      if (controlSendTimer !== null) {
        clearTimeout(controlSendTimer);
      }

      controlSendTimer = setTimeout(function() {
        controlSendTimer = null;
        sendControlState();
      }, CONTROL_DEBOUNCE_MS);
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
      ws.send(JSON.stringify({cmd: 'getStatus'}));
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

          if (typeof data.servoUpdown === 'number') {
            servoUpdownSlider.value = data.servoUpdown;
            servoUpdownValue.textContent = data.servoUpdown;
          }

          if (typeof data.servoLateral === 'number') {
            servoLateralSlider.value = data.servoLateral;
            servoLateralValue.textContent = data.servoLateral;
      }

      if (typeof data.motorPwm === 'number') {
        motorSlider.value = data.motorPwm;
        motorValue.textContent = data.motorPwm;
      }

      if (typeof data.motorDir === 'number') {
        setDirButtons(data.motorDir);
      }

      statusSynced = true;
      if (controlStateDirty) {
        scheduleControlStateSend();
      }
    };

    servoUpdownSlider.addEventListener('input', function() {
      servoUpdownValue.textContent = servoUpdownSlider.value;
      controlStateDirty = true;
      if (statusSynced) {
        scheduleControlStateSend();
      }
    });

    servoLateralSlider.addEventListener('input', function() {
      servoLateralValue.textContent = servoLateralSlider.value;
      controlStateDirty = true;
      if (statusSynced) {
        scheduleControlStateSend();
      }
    });

    motorSlider.addEventListener('input', function() {
      motorValue.textContent = motorSlider.value;
      controlStateDirty = true;
      if (statusSynced) {
        scheduleControlStateSend();
      }
    });

    dirCw.addEventListener('click', function() {
      setDirButtons(0);
      controlStateDirty = true;
      if (statusSynced) {
        scheduleControlStateSend();
      }
    });

    dirCcw.addEventListener('click', function() {
      setDirButtons(1);
      controlStateDirty = true;
      if (statusSynced) {
        scheduleControlStateSend();
      }
    });

    // Optional: Initial-Status abfragen
    window.addEventListener('load', function() {
      setDirButtons(0);
      servoUpdownValue.textContent = servoUpdownSlider.value;
      servoLateralValue.textContent = servoLateralSlider.value;
      motorValue.textContent = motorSlider.value;
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

int16_t clampServoOffsetValue(int16_t servoOffsetValue, int16_t servoLimit) {
  return constrain(servoOffsetValue, -servoLimit, servoLimit);
}

uint16_t offsetServoValueToTargetPos(int16_t servoOffsetValue, int16_t servoLimit) {
  int16_t clampedOffset = clampServoOffsetValue(servoOffsetValue, servoLimit);
  int32_t targetPos = static_cast<int32_t>(SERVO_POS_CENTER) + clampedOffset;
  return (uint16_t)constrain(targetPos, (int32_t)SERVO_POS_MIN, (int32_t)SERVO_POS_MAX);
}

int readServoCurrentPosition(uint8_t servoId) {
  int currentPos = scServo.ReadPos(servoId);
  if (scServo.getErr() != 0) {
    return -1;
  }
  return currentPos;
}

void applyServoFromUiValue(int16_t servoOffsetValue, uint8_t servoId, int16_t &currentServoValue, int16_t servoLimit) {
  int16_t clampedValue = clampServoOffsetValue(servoOffsetValue, servoLimit);

  if (clampedValue == currentServoValue) {
    return;
  }

  // Update current state and desired position for the servo task to process.
  currentServoValue = clampedValue;
  portENTER_CRITICAL(&servoMux);
  if (servoId == SERVO_UPDOWN_ID) {
    desiredServoUpdownValue = clampedValue;
  } else if (servoId == SERVO_LATERAL_ID) {
    desiredServoLateralValue = clampedValue;
  }
  portEXIT_CRITICAL(&servoMux);
}

// FreeRTOS task that serializes and throttles SCServo writes to avoid blocking network tasks
void servoTask(void *pvParameters) {
  int16_t lastSentUpdown = 0x7FFF; // sentinel
  int16_t lastSentLateral = 0x7FFF;

  for (;;) {
    int16_t wantUpdown;
    int16_t wantLateral;

    portENTER_CRITICAL(&servoMux);
    wantUpdown = desiredServoUpdownValue;
    wantLateral = desiredServoLateralValue;
    portEXIT_CRITICAL(&servoMux);

    if (wantUpdown != lastSentUpdown) {
      uint16_t targetPos = offsetServoValueToTargetPos(wantUpdown, updown_pos_limit);
      int lastPosRead = readServoCurrentPosition(SERVO_UPDOWN_ID);
      if (lastPosRead >= 0) {
        Serial.printf("[SERVO UPDOWN] last pos read=%d bits, current pos command=%u bits\n", lastPosRead, targetPos);
      } else {
        Serial.printf("[SERVO UPDOWN] last pos read=read_error, current pos command=%u bits\n", targetPos);
      }
      scServo.WritePos(SERVO_UPDOWN_ID, targetPos, 0, SERVO_SPEED);
      int currentPosRead = readServoCurrentPosition(SERVO_UPDOWN_ID);
      if (currentPosRead >= 0) {
        Serial.printf("[SERVO UPDOWN] current pos read=%d bits\n", currentPosRead);
      } else {
        Serial.println("[SERVO UPDOWN] current pos read=read_error");
      }
      lastSentUpdown = wantUpdown;
    }

    if (wantLateral != lastSentLateral) {
      uint16_t targetPos = offsetServoValueToTargetPos(wantLateral, lateral_pos_limit);
      int lastPosRead = readServoCurrentPosition(SERVO_LATERAL_ID);
      if (lastPosRead >= 0) {
        Serial.printf("[SERVO LATERAL] last pos read=%d bits, current pos command=%u bits\n", lastPosRead, targetPos);
      } else {
        Serial.printf("[SERVO LATERAL] last pos read=read_error, current pos command=%u bits\n", targetPos);
      }
      scServo.WritePos(SERVO_LATERAL_ID, targetPos, 0, SERVO_SPEED);
      int currentPosRead = readServoCurrentPosition(SERVO_LATERAL_ID);
      if (currentPosRead >= 0) {
        Serial.printf("[SERVO LATERAL] current pos read=%d bits\n", currentPosRead);
      } else {
        Serial.println("[SERVO LATERAL] current pos read=read_error");
      }
      lastSentLateral = wantLateral;
    }

    // Delay a bit to yield to other tasks and avoid watchdog issues
    vTaskDelay(pdMS_TO_TICKS(25));
  }
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

      int servoUpdownValue = currentServoUpdownValue;
      int servoLateralValue = currentServoLateralValue;
      bool servoUpdownChanged = false;
      bool servoLateralChanged = false;

      if (parseJsonIntInRange(buffer, "servoUpdown", -updown_pos_limit, updown_pos_limit, &servoUpdownValue)) {
        servoUpdownChanged = true;
      }
      if (parseJsonIntInRange(buffer, "servoLateral", -lateral_pos_limit, lateral_pos_limit, &servoLateralValue)) {
        servoLateralChanged = true;
      }

      if (servoUpdownChanged && servoUpdownValue != currentServoUpdownValue) {
        Serial.printf("[SERVO UPDOWN] %d -> %d\n", currentServoUpdownValue, servoUpdownValue);
        applyServoFromUiValue((int16_t)servoUpdownValue, SERVO_UPDOWN_ID, currentServoUpdownValue, updown_pos_limit);
      }

      if (servoLateralChanged && servoLateralValue != currentServoLateralValue) {
        Serial.printf("[SERVO LATERAL] %d -> %d\n", currentServoLateralValue, servoLateralValue);
        applyServoFromUiValue((int16_t)servoLateralValue, SERVO_LATERAL_ID, currentServoLateralValue, lateral_pos_limit);
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
        snprintf(statusMsg, sizeof(statusMsg), "{\"servoUpdown\":%d,\"servoLateral\":%d,\"motorPwm\":%u,\"motorDir\":%u,\"nodeConnected\":%s}",
                 currentServoUpdownValue, currentServoLateralValue, currentMotorPwm, currentMotorDir,
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
  applyServoFromUiValue(currentServoUpdownValue, SERVO_UPDOWN_ID, currentServoUpdownValue, updown_pos_limit);
  applyServoFromUiValue(currentServoLateralValue, SERVO_LATERAL_ID, currentServoLateralValue, lateral_pos_limit);
  // Start servo task to handle actual SCServo writes outside network/event context
  xTaskCreatePinnedToCore(servoTask, "servoTask", 4096, NULL, 1, NULL, 0);
  Serial.printf("[SERVO] Updown ID %u, Lateral ID %u\n", SERVO_UPDOWN_ID, SERVO_LATERAL_ID);

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

