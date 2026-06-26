#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SCServo.h>

// ============ KONFIGURATION ============
const char* WIFI_SSID = "ESP_TH";
const char* WIFI_PASS = "TurningHeads123";
const uint16_t TCP_PORT = 5000;
const uint16_t NODE_PORT_BASE = 5000;
const uint8_t NODE_COUNT = 3;
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
WiFiServer tcpServer1(NODE_PORT_BASE + 1);
WiFiServer tcpServer2(NODE_PORT_BASE + 2);
WiFiServer tcpServer3(NODE_PORT_BASE + 3);
WiFiClient tcpClient1;
WiFiClient tcpClient2;
WiFiClient tcpClient3;
uint8_t currentMotorPwm = 0;    // 0..255
uint8_t currentMotorDir = 0;    // 0=CW, 1=CCW
int16_t currentServoUpdownValue = 0;    // -80..80, 0 = Mitte (511 bits)
int16_t currentServoLateralValue = 0;  // -80..80, 0 = Mitte (511 bits)
SCSCL scServo;
// Desired servo positions written by WebSocket handler; actual writes happen in servoTask
volatile int16_t desiredServoUpdownValue = 0;
volatile int16_t desiredServoLateralValue = 0;
portMUX_TYPE servoMux = portMUX_INITIALIZER_UNLOCKED;

enum NodeTestState {
  NODE_TEST_IDLE,
  NODE_TEST_SENDING,
  NODE_TEST_OK,
  NODE_TEST_ERROR
};

struct NodeSlot {
  uint8_t id;
  WiFiServer* server;
  WiFiClient* client;
  bool connected;
  NodeTestState testState;
  bool testPending;
  uint32_t lastSeenMs;
  uint32_t lastTestMs;
  char testMessage[32];
};

NodeSlot nodeSlots[NODE_COUNT] = {
  {1, &tcpServer1, &tcpClient1, false, NODE_TEST_IDLE, false, 0, 0, "Bereit"},
  {2, &tcpServer2, &tcpClient2, false, NODE_TEST_IDLE, false, 0, 0, "Bereit"},
  {3, &tcpServer3, &tcpClient3, false, NODE_TEST_IDLE, false, 0, 0, "Bereit"}
};

const uint32_t NODE_HEARTBEAT_TIMEOUT_MS = 4000;
const uint32_t NODE_TEST_TIMEOUT_MS = 2500;

const char* nodeTestStateToString(NodeTestState state) {
  switch (state) {
    case NODE_TEST_SENDING: return "sending";
    case NODE_TEST_OK: return "ok";
    case NODE_TEST_ERROR: return "error";
    case NODE_TEST_IDLE:
    default: return "idle";
  }
}

NodeSlot* getNodeSlotById(uint8_t nodeId) {
  for (uint8_t index = 0; index < NODE_COUNT; ++index) {
    if (nodeSlots[index].id == nodeId) {
      return &nodeSlots[index];
    }
  }
  return nullptr;
}

void setNodeTestState(NodeSlot &node, NodeTestState state, const char* message) {
  node.testState = state;
  strncpy(node.testMessage, message, sizeof(node.testMessage) - 1);
  node.testMessage[sizeof(node.testMessage) - 1] = '\0';
}

void markNodeSeen(NodeSlot &node) {
  node.lastSeenMs = millis();
  node.connected = true;
}

uint8_t countConnectedNodes() {
  uint8_t count = 0;
  for (uint8_t index = 0; index < NODE_COUNT; ++index) {
    if (nodeSlots[index].connected) {
      ++count;
    }
  }
  return count;
}

String buildStatusMessage() {
  String statusMsg = "{";
  statusMsg += "\"servoUpdown\":";
  statusMsg += currentServoUpdownValue;
  statusMsg += ",\"servoLateral\":";
  statusMsg += currentServoLateralValue;
  statusMsg += ",\"motorPwm\":";
  statusMsg += currentMotorPwm;
  statusMsg += ",\"motorDir\":";
  statusMsg += currentMotorDir;
  statusMsg += ",\"connectedNodes\":";
  statusMsg += countConnectedNodes();
  statusMsg += ",\"nodeCount\":";
  statusMsg += NODE_COUNT;
  statusMsg += ",\"nodeConnected\":";
  statusMsg += (countConnectedNodes() > 0) ? "true" : "false";
  statusMsg += ",\"nodes\":[";

  for (uint8_t index = 0; index < NODE_COUNT; ++index) {
    NodeSlot &node = nodeSlots[index];
    statusMsg += "{";
    statusMsg += "\"id\":";
    statusMsg += node.id;
    statusMsg += ",\"connected\":";
    statusMsg += node.connected ? "true" : "false";
    statusMsg += ",\"lastSeenMs\":";
    statusMsg += node.lastSeenMs;
    statusMsg += ",\"testState\":\"";
    statusMsg += nodeTestStateToString(node.testState);
    statusMsg += "\",\"testMessage\":\"";
    statusMsg += node.testMessage;
    statusMsg += "\"}";
    if (index + 1 < NODE_COUNT) {
      statusMsg += ",";
    }
  }

  statusMsg += "]}";
  return statusMsg;
}

void broadcastStatus() {
  ws.textAll(buildStatusMessage());
}

void sendNodeTestSignal(NodeSlot &node) {
  if (!node.connected || !node.client->connected()) {
    setNodeTestState(node, NODE_TEST_ERROR, "Node nicht verbunden");
    node.testPending = false;
    broadcastStatus();
    return;
  }

  node.client->println("TEST");
  node.lastTestMs = millis();
  node.testPending = true;
  setNodeTestState(node, NODE_TEST_SENDING, "Signal gesendet");
  broadcastStatus();
}

void updateNodeConnection(NodeSlot &node, bool connected, const char* message) {
  node.connected = connected;
  if (!connected) {
    node.client->stop();
    node.testPending = false;
    setNodeTestState(node, NODE_TEST_ERROR, message);
  } else {
    setNodeTestState(node, NODE_TEST_IDLE, message);
  }
  broadcastStatus();
}

void serviceNodeSlot(NodeSlot &node) {
  if (!node.client->connected()) {
    WiFiClient newClient = node.server->available();
    if (newClient) {
      *node.client = newClient;
      updateNodeConnection(node, true, "Node bereit");
      Serial.printf("[TCP] Node %u connected on port %u\n", node.id, (unsigned)(NODE_PORT_BASE + node.id));
    }
    return;
  }

  while (node.client->available()) {
    String line = node.client->readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      continue;
    }

    Serial.printf("[TCP RX node %u] %s\n", node.id, line.c_str());

    if (line.startsWith("NODE_READY") || line.startsWith("HELLO")) {
      markNodeSeen(node);
      updateNodeConnection(node, true, "Node bereit");
      continue;
    }

    if (line.startsWith("PONG") || line.startsWith("TEST_OK")) {
      markNodeSeen(node);
      node.testPending = false;
      setNodeTestState(node, NODE_TEST_OK, "Antwort vom Node");
      broadcastStatus();
      continue;
    }

    if (line.startsWith("HEARTBEAT") || line.startsWith("ALIVE")) {
      markNodeSeen(node);
      if (!node.connected) {
        updateNodeConnection(node, true, "Node bereit");
      } else {
        broadcastStatus();
      }
      continue;
    }
  }

  if (!node.client->connected()) {
    if (node.connected) {
      updateNodeConnection(node, false, "Verbindung verloren");
      Serial.printf("[TCP] Node %u disconnected\n", node.id);
    }
  }
}

void checkNodeHealth(NodeSlot &node) {
  if (node.connected && node.lastSeenMs != 0 && (millis() - node.lastSeenMs) > NODE_HEARTBEAT_TIMEOUT_MS) {
    updateNodeConnection(node, false, "Kein Heartbeat");
    Serial.printf("[TCP] Node %u heartbeat timeout\n", node.id);
    return;
  }

  if (node.testPending && (millis() - node.lastTestMs) > NODE_TEST_TIMEOUT_MS) {
    node.testPending = false;
    setNodeTestState(node, NODE_TEST_ERROR, "Keine Antwort");
    broadcastStatus();
  }
}

// ============ HILFSFUNKTION: HTML/CSS/JS WEB-UI ============
const char* getWebPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>TurningHeads</title>
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
    .joystick-panel {
      margin: 20px 0 10px;
    }
    .joystick-readout {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
      margin-bottom: 12px;
      font-weight: bold;
    }
    .joystick-readout div {
      background: #2a2a2a;
      border: 1px solid #4b4b4b;
      border-radius: 8px;
      padding: 10px 12px;
    }
    .joystick-pad {
      position: relative;
      width: min(72vw, 280px);
      aspect-ratio: 1 / 1;
      margin: 0 auto;
      border-radius: 18px;
      border: 1px solid #555;
      background:
        radial-gradient(circle at center, rgba(76, 175, 80, 0.16), transparent 45%),
        linear-gradient(90deg, transparent 49.5%, rgba(255,255,255,0.12) 50%, transparent 50.5%),
        linear-gradient(0deg, transparent 49.5%, rgba(255,255,255,0.12) 50%, transparent 50.5%),
        #1f1f1f;
      touch-action: none;
      user-select: none;
      -webkit-user-select: none;
    }
    .joystick-thumb {
      position: absolute;
      left: 50%;
      top: 50%;
      width: 54px;
      height: 54px;
      margin-left: -27px;
      margin-top: -27px;
      border-radius: 50%;
      background: radial-gradient(circle at 30% 30%, #7ae57e, #2d9f3b 55%, #1b6f28 100%);
      box-shadow: 0 10px 24px rgba(0, 0, 0, 0.35);
      transform: translate(0px, 0px);
      transition: transform 40ms linear;
      pointer-events: none;
    }
    .joystick-hint {
      margin-top: 10px;
      color: #bdbdbd;
      font-size: 13px;
    }
    .node-grid {
      margin-top: 18px;
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(170px, 1fr));
      gap: 12px;
    }
    .node-card {
      border: 1px solid rgba(255,255,255,0.08);
      border-radius: 12px;
      padding: 14px;
      background: #2b2b2b;
      text-align: left;
      display: flex;
      flex-direction: column;
      gap: 10px;
    }
    .node-card-header {
      font-weight: bold;
      color: #fff;
    }
    .node-card-status {
      display: inline-flex;
      align-items: center;
      gap: 10px;
      font-weight: bold;
      color: #f2f2f2;
    }
    .node-card-message {
      color: #bdbdbd;
      font-size: 13px;
      min-height: 18px;
    }
    .node-card button {
      border: 0;
      border-radius: 10px;
      padding: 12px 16px;
      font-weight: bold;
      background: #4CAF50;
      color: #111;
    }
    .node-card button:active {
      transform: translateY(1px);
    }
    .signal-light {
      width: 16px;
      height: 16px;
      border-radius: 50%;
      background: #666;
      box-shadow: 0 0 0 3px rgba(255,255,255,0.06);
    }
    .signal-idle { background: #666; }
    .signal-sending { background: #ffb300; box-shadow: 0 0 14px rgba(255, 179, 0, 0.7); }
    .signal-ok { background: #4CAF50; box-shadow: 0 0 14px rgba(76, 175, 80, 0.8); }
    .signal-error { background: #f44336; box-shadow: 0 0 14px rgba(244, 67, 54, 0.75); }
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
    
    <div class="joystick-panel">
      <div class="joystick-readout">
        <div>Updown: <span id="servoUpdownValue">0</span></div>
        <div>Lateral: <span id="servoLateralValue">0</span></div>
      </div>
      <div id="joystickPad" class="joystick-pad" aria-label="Servo joystick">
        <div id="joystickThumb" class="joystick-thumb"></div>
      </div>
      <div class="joystick-hint">Drag to set both axes. Position stays where you leave it.</div>
    </div>

    <label>DC Motor PWM (0..255)</label>
    <input type="range" id="motorSlider" min="0" max="255" value="0">
    <div id="motorValue">0</div>

    <label>DC Motor Richtung</label>
    <div>
      <button id="dirCw" type="button">CW</button>
      <button id="dirCcw" type="button">CCW</button>
    </div>

    <div class="node-grid">
      <div class="node-card">
        <div class="node-card-header">Node 1</div>
        <div class="node-card-status">
          <span id="node1Light" class="signal-light signal-idle"></span>
          <span id="node1Connection">Disconnected</span>
        </div>
        <button id="node1TestButton" type="button">Testsignal senden</button>
        <div id="node1Message" class="node-card-message">Bereit</div>
      </div>
      <div class="node-card">
        <div class="node-card-header">Node 2</div>
        <div class="node-card-status">
          <span id="node2Light" class="signal-light signal-idle"></span>
          <span id="node2Connection">Disconnected</span>
        </div>
        <button id="node2TestButton" type="button">Testsignal senden</button>
        <div id="node2Message" class="node-card-message">Bereit</div>
      </div>
      <div class="node-card">
        <div class="node-card-header">Node 3</div>
        <div class="node-card-status">
          <span id="node3Light" class="signal-light signal-idle"></span>
          <span id="node3Connection">Disconnected</span>
        </div>
        <button id="node3TestButton" type="button">Testsignal senden</button>
        <div id="node3Message" class="node-card-message">Bereit</div>
      </div>
    </div>

    <div id="status">
      <div>WebSocket: <span id="wsStatus" class="status-error">Disconnected</span></div>
      <div>Node: <span id="nodeStatus" class="status-error">No signal</span></div>
    </div>
  </div>

  <script>
    // === WebSocket Verbindung ===
    const ws = new WebSocket('ws://' + window.location.host + '/ws');
    const servoUpdownValue = document.getElementById('servoUpdownValue');
    const servoLateralValue = document.getElementById('servoLateralValue');
    const joystickPad = document.getElementById('joystickPad');
    const joystickThumb = document.getElementById('joystickThumb');
    const motorSlider = document.getElementById('motorSlider');
    const motorValue = document.getElementById('motorValue');
    const dirCw = document.getElementById('dirCw');
    const dirCcw = document.getElementById('dirCcw');
    const wsStatus = document.getElementById('wsStatus');
    const nodeStatus = document.getElementById('nodeStatus');
    const UPDOWN_LIMIT = 140;
    const LATERAL_LIMIT = 80;
    let currentDir = 0;
    let currentServoUpdown = 0;
    let currentServoLateral = 0;
    let controlSendTimer = null;
    let statusSynced = false;
    let controlStateDirty = false;
    const nodeUi = [1, 2, 3].map(function(nodeId) {
      return {
        id: nodeId,
        button: document.getElementById('node' + nodeId + 'TestButton'),
        light: document.getElementById('node' + nodeId + 'Light'),
        connection: document.getElementById('node' + nodeId + 'Connection'),
        message: document.getElementById('node' + nodeId + 'Message')
      };
    });

    function clamp(value, min, max) {
      return Math.min(max, Math.max(min, value));
    }

    function syncJoystickReadout() {
      servoUpdownValue.textContent = currentServoUpdown;
      servoLateralValue.textContent = currentServoLateral;
    }

    function renderJoystickThumb() {
      const rect = joystickPad.getBoundingClientRect();
      const thumbSize = joystickThumb.offsetWidth || 54;
      const xRange = Math.max(0, rect.width / 2 - thumbSize / 2);
      const yRange = Math.max(0, rect.height / 2 - thumbSize / 2);
      const xOffset = clamp(currentServoLateral / LATERAL_LIMIT, -1, 1) * xRange;
      const yOffset = clamp(-currentServoUpdown / UPDOWN_LIMIT, -1, 1) * yRange;
      joystickThumb.style.transform = `translate(${xOffset}px, ${yOffset}px)`;
    }

    function setJoystickValues(updown, lateral, shouldSend) {
      currentServoUpdown = clamp(parseInt(updown, 10) || 0, -UPDOWN_LIMIT, UPDOWN_LIMIT);
      currentServoLateral = clamp(parseInt(lateral, 10) || 0, -LATERAL_LIMIT, LATERAL_LIMIT);
      syncJoystickReadout();
      renderJoystickThumb();

      if (shouldSend) {
        controlStateDirty = true;
        if (statusSynced) {
          scheduleControlStateSend();
        }
      }
    }

    function updateJoystickFromPointer(clientX, clientY, shouldSend) {
      const rect = joystickPad.getBoundingClientRect();
      const xHalf = Math.max(1, rect.width / 2);
      const yHalf = Math.max(1, rect.height / 2);
      const xNorm = clamp((clientX - (rect.left + xHalf)) / xHalf, -1, 1);
      const yNorm = clamp((clientY - (rect.top + yHalf)) / yHalf, -1, 1);

      setJoystickValues(Math.round(-yNorm * UPDOWN_LIMIT), Math.round(xNorm * LATERAL_LIMIT), shouldSend);
    }

    function buildControlState() {
      return {
        servoUpdown: currentServoUpdown,
        servoLateral: currentServoLateral,
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
      sendControlState();
      controlStateDirty = false;
    }

    function setDirButtons(dir) {
      currentDir = dir;
      dirCw.style.background = dir === 0 ? '#4CAF50' : '#555';
      dirCcw.style.background = dir === 1 ? '#4CAF50' : '#555';
    }

    function setNodeUi(nodeId, connected, state, message) {
      const node = nodeUi.find(function(entry) {
        return entry.id === nodeId;
      });

      if (!node) {
        return;
      }

      node.light.className = 'signal-light signal-' + state;
      node.connection.textContent = connected ? 'Connected' : 'Disconnected';
      node.connection.className = connected ? 'status-ok' : 'status-error';
      node.message.textContent = message;
    }

    function requestNodeTest(nodeId) {
      if (ws.readyState !== WebSocket.OPEN) {
        setNodeUi(nodeId, false, 'error', 'WebSocket getrennt');
        return;
      }

      setNodeUi(nodeId, true, 'sending', 'Signal wird gesendet...');
      ws.send(JSON.stringify({cmd: 'nodeTest', nodeId: nodeId}));
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
      const nodeCount = typeof data.nodeCount === 'number' ? data.nodeCount : 3;
      const connectedNodes = typeof data.connectedNodes === 'number'
        ? data.connectedNodes
        : (Array.isArray(data.nodes) ? data.nodes.filter(function(node) { return !!node.connected; }).length : 0);
      nodeStatus.textContent = connectedNodes + '/' + nodeCount + ' connected';
      nodeStatus.className = connectedNodes > 0 ? 'status-ok' : 'status-error';

          if (typeof data.servoUpdown === 'number' || typeof data.servoLateral === 'number') {
            setJoystickValues(
              typeof data.servoUpdown === 'number' ? data.servoUpdown : currentServoUpdown,
              typeof data.servoLateral === 'number' ? data.servoLateral : currentServoLateral,
              false
            );
      }

      if (typeof data.motorPwm === 'number') {
        motorSlider.value = data.motorPwm;
        motorValue.textContent = data.motorPwm;
      }

      if (typeof data.motorDir === 'number') {
        setDirButtons(data.motorDir);
      }

      if (Array.isArray(data.nodes)) {
        data.nodes.forEach(function(node) {
          setNodeUi(
            node.id,
            !!node.connected,
            typeof node.testState === 'string' ? node.testState : 'idle',
            typeof node.testMessage === 'string' ? node.testMessage : 'Bereit'
          );
        });
      }

      statusSynced = true;
      if (controlStateDirty) {
        scheduleControlStateSend();
      }
    };

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

    nodeUi.forEach(function(node) {
      node.button.addEventListener('click', function() {
        requestNodeTest(node.id);
      });
    });

    // Optional: Initial-Status abfragen
    window.addEventListener('load', function() {
      setDirButtons(0);
      setJoystickValues(0, 0, false);
      motorValue.textContent = motorSlider.value;
      nodeUi.forEach(function(node) {
        setNodeUi(node.id, false, 'idle', 'Bereit');
      });
    });

    let joystickDragging = false;

    joystickPad.addEventListener('pointerdown', function(event) {
      joystickDragging = true;
      joystickPad.setPointerCapture(event.pointerId);
      updateJoystickFromPointer(event.clientX, event.clientY, true);
      event.preventDefault();
    });

    joystickPad.addEventListener('pointermove', function(event) {
      if (!joystickDragging) {
        return;
      }

      updateJoystickFromPointer(event.clientX, event.clientY, true);
      event.preventDefault();
    });

    function finishJoystickDrag(event) {
      if (!joystickDragging) {
        return;
      }

      joystickDragging = false;
      if (joystickPad.hasPointerCapture(event.pointerId)) {
        joystickPad.releasePointerCapture(event.pointerId);
      }
    }

    joystickPad.addEventListener('pointerup', finishJoystickDrag);
    joystickPad.addEventListener('pointercancel', finishJoystickDrag);
    window.addEventListener('resize', renderJoystickThumb);

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
      scServo.WritePos(SERVO_UPDOWN_ID, targetPos, 0, SERVO_SPEED);
      Serial.printf("[SERVO UPDOWN] command=%u bits\n", targetPos);
      lastSentUpdown = wantUpdown;
    }

    if (wantLateral != lastSentLateral) {
      uint16_t targetPos = offsetServoValueToTargetPos(wantLateral, lateral_pos_limit);
      scServo.WritePos(SERVO_LATERAL_ID, targetPos, 0, SERVO_SPEED);
      Serial.printf("[SERVO LATERAL] command=%u bits\n", targetPos);
      lastSentLateral = wantLateral;
    }

    // Delay a bit to yield to other tasks and avoid watchdog issues
    vTaskDelay(pdMS_TO_TICKS(5));
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

      if (strstr(buffer, "\"nodeTest\"") != NULL) {
        int nodeId = -1;
        if (parseJsonIntInRange(buffer, "nodeId", 1, NODE_COUNT, &nodeId)) {
          NodeSlot* node = getNodeSlotById((uint8_t)nodeId);
          if (node != nullptr) {
            sendNodeTestSignal(*node);
          }
        }
        return;
      }

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

        char tcpMsg[20];
        snprintf(tcpMsg, sizeof(tcpMsg), "M:%d\n", currentMotorPwm);
        for (uint8_t index = 0; index < NODE_COUNT; ++index) {
          if (nodeSlots[index].connected && nodeSlots[index].client->connected()) {
            nodeSlots[index].client->print(tcpMsg);
          }
        }
      }

      if (strstr(buffer, "\"getStatus\"") != NULL) {
        client->text(buildStatusMessage());
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
  for (uint8_t index = 0; index < NODE_COUNT; ++index) {
    nodeSlots[index].server->begin();
    Serial.printf("[TCP] Listening on port %u for Node %u\n", (unsigned)(NODE_PORT_BASE + nodeSlots[index].id), nodeSlots[index].id);
  }

  broadcastStatus();
}

// ============ LOOP ============
void loop() {
  for (uint8_t index = 0; index < NODE_COUNT; ++index) {
    serviceNodeSlot(nodeSlots[index]);
    checkNodeHealth(nodeSlots[index]);
  }

  delay(10);
}

