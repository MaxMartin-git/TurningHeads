#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <ESPAsyncWebServer.h>
#include <SCServo.h>
#include "shared/th_roles.h"
#include "shared/th_input_modes.h"
#include "shared/th_protocol.h"
#include "coordinator/th_wifi_ap.h"
#include "coordinator/th_json_utils.h"
#include "coordinator/th_web_page.h"

// ============ KONFIGURATION ============
const char* WIFI_SSID = "ESP_TH";
const char* WIFI_PASS = "TurningHeads123";
const uint16_t NODE_PORT_BASE = 5000;
const uint8_t NODE_COUNT = 3;
const uint16_t MOTOR_PWM_PIN = 3;  // PWM-Eingang am Motortreiber
const uint16_t MOTOR_DIR_PIN = 4;  // DIR-Eingang am Motortreiber
const uint8_t SERVO_UART_TX_PIN = 21;
const uint8_t SERVO_UART_RX_PIN = 20;
const uint32_t SERVO_BAUD = 1000000;  // SC09 braucht 1 Mbps
const uint8_t SERVO_UPDOWN_ID = 1;
const uint8_t SERVO_LATERAL_ID = 2;
const uint16_t SERVO_POS_MIN = 0;
const uint16_t SERVO_POS_MAX = 1023;
const uint16_t SERVO_POS_CENTER = 511;
const uint8_t AS5600_I2C_ADDR = 0x36;
const uint8_t AS5600_RAW_ANGLE_REG = 0x0C;
const uint8_t AS5600_SDA_PIN = 8;
const uint8_t AS5600_SCL_PIN = 9;
const uint32_t AS5600_POLL_INTERVAL_MS = 20;
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

struct SideControlState {
  uint8_t motorPwm;
  uint8_t motorDir;
  int16_t servoUpdown;
  int16_t servoLateral;
  bool lightOn;
  bool lightBreakerActive;
  bool fogOn;
};

SideControlState sideState[2] = {
  {0, 0, 0, 0, false, false, false},  // Left side (local base/coordinator)
  {0, 0, 0, 0, false, false, false}   // Right side (remote base/satellite)
};

bool motorLinkEnabled = true;
bool motorMirrorEnabled = true;
bool eyeballLinkEnabled = true;
bool eyeballMirrorEnabled = true;
bool lightLinkEnabled = true;
bool fogLinkEnabled = true;

th::InputMode currentInputMode = th::InputMode::Manual;
uint16_t activeSequenceId = 0;
uint16_t activeSequenceStep = 0;
SCSCL scServo;
// Desired servo positions written by WebSocket handler; actual writes happen in servoTask
volatile int16_t desiredServoUpdownValue = 0;
volatile int16_t desiredServoLateralValue = 0;
portMUX_TYPE servoMux = portMUX_INITIALIZER_UNLOCKED;
int leftBaseAngleRaw = -1;
int rightBaseAngleRaw = -1;
int leftBaseAngleDeg10 = -1;
int rightBaseAngleDeg10 = -1;
uint32_t lastAs5600PollMs = 0;

enum NodeTestState {
  NODE_TEST_IDLE,
  NODE_TEST_SENDING,
  NODE_TEST_OK,
  NODE_TEST_ERROR
};

struct NodeSlot {
  uint8_t id;
  th::DeviceProfile profile;
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
  {1, th::getRemoteNodeProfile(1), &tcpServer1, &tcpClient1, false, NODE_TEST_IDLE, false, 0, 0, "Ready"},
  {2, th::getRemoteNodeProfile(2), &tcpServer2, &tcpClient2, false, NODE_TEST_IDLE, false, 0, 0, "Ready"},
  {3, th::getRemoteNodeProfile(3), &tcpServer3, &tcpClient3, false, NODE_TEST_IDLE, false, 0, 0, "Ready"}
};
NodeTestState hostTestState = NODE_TEST_IDLE;
char hostTestMessage[32] = "Ready";

void broadcastStatus();

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
  // Host node (Coordinator/Base_L) is local and always reachable while UI is connected.
  ++count;
  for (uint8_t index = 0; index < NODE_COUNT; ++index) {
    if (nodeSlots[index].connected) {
      ++count;
    }
  }
  return count;
}

SideControlState& getSideState(th::Side side) {
  return sideState[th::sideToIndex(side)];
}

const SideControlState& getSideStateConst(th::Side side) {
  return sideState[th::sideToIndex(side)];
}

th::Side oppositeSide(th::Side side) {
  return side == th::Side::Right ? th::Side::Left : th::Side::Right;
}

int16_t rawAngleToDeg10(uint16_t rawAngle) {
  uint32_t scaled = ((uint32_t)rawAngle * 3600UL + 2048UL) / 4096UL;
  if (scaled >= 3600UL) {
    scaled = 0;
  }
  return static_cast<int16_t>(scaled);
}

bool readAs5600RawAngle(uint16_t* outRawAngle) {
  Wire.beginTransmission(AS5600_I2C_ADDR);
  Wire.write(AS5600_RAW_ANGLE_REG);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  uint8_t bytes = Wire.requestFrom((int)AS5600_I2C_ADDR, 2);
  if (bytes != 2) {
    return false;
  }

  uint8_t highByte = Wire.read();
  uint8_t lowByte = Wire.read();
  *outRawAngle = static_cast<uint16_t>(((highByte & 0x0F) << 8) | lowByte);
  return true;
}

void pollLocalBaseAs5600() {
  if ((millis() - lastAs5600PollMs) < AS5600_POLL_INTERVAL_MS) {
    return;
  }
  lastAs5600PollMs = millis();

  uint16_t rawAngle = 0;
  if (!readAs5600RawAngle(&rawAngle)) {
    if (leftBaseAngleRaw != -1 || leftBaseAngleDeg10 != -1) {
      leftBaseAngleRaw = -1;
      leftBaseAngleDeg10 = -1;
      broadcastStatus();
    }
    return;
  }

  int16_t deg10Angle = rawAngleToDeg10(rawAngle);
  if ((int)rawAngle != leftBaseAngleRaw || (int)deg10Angle != leftBaseAngleDeg10) {
    leftBaseAngleRaw = (int)rawAngle;
    leftBaseAngleDeg10 = (int)deg10Angle;
    broadcastStatus();
  }
}

void setHostTestState(NodeTestState state, const char* message) {
  hostTestState = state;
  strncpy(hostTestMessage, message, sizeof(hostTestMessage) - 1);
  hostTestMessage[sizeof(hostTestMessage) - 1] = '\0';
}

void runHostDeviceTest() {
  setHostTestState(NODE_TEST_SENDING, "Testing host endpoint");
  broadcastStatus();

  // Local host test intentionally verifies only endpoint responsiveness, not peripherals.
  setHostTestState(NODE_TEST_OK, "Host endpoint OK");
  broadcastStatus();
}

uint8_t mirroredMotorDir(uint8_t dirValue) {
  return dirValue == 0 ? 1 : 0;
}

int16_t mirrorEyeballLateral(int16_t value) {
  return static_cast<int16_t>(-value);
}

String buildStatusMessage() {
  const SideControlState& left = getSideStateConst(th::Side::Left);
  const SideControlState& right = getSideStateConst(th::Side::Right);

  String statusMsg = "{";
  statusMsg += "\"inputMode\":\"";
  statusMsg += (currentInputMode == th::InputMode::Manual) ? "manual" : "sequence";
  statusMsg += "\",\"sequenceId\":";
  statusMsg += activeSequenceId;
  statusMsg += ",\"sequenceStep\":";
  statusMsg += activeSequenceStep;
  statusMsg += ",";
  statusMsg += "\"leftServoUpdown\":";
  statusMsg += left.servoUpdown;
  statusMsg += ",\"leftServoLateral\":";
  statusMsg += left.servoLateral;
  statusMsg += ",\"leftMotorPwm\":";
  statusMsg += left.motorPwm;
  statusMsg += ",\"leftMotorDir\":";
  statusMsg += left.motorDir;
  statusMsg += ",\"rightServoUpdown\":";
  statusMsg += right.servoUpdown;
  statusMsg += ",\"rightServoLateral\":";
  statusMsg += right.servoLateral;
  statusMsg += ",\"rightMotorPwm\":";
  statusMsg += right.motorPwm;
  statusMsg += ",\"rightMotorDir\":";
  statusMsg += right.motorDir;
  statusMsg += ",\"leftBaseAngleRaw\":";
  statusMsg += leftBaseAngleRaw;
  statusMsg += ",\"rightBaseAngleRaw\":";
  statusMsg += rightBaseAngleRaw;
  statusMsg += ",\"leftBaseAngleDeg10\":";
  statusMsg += leftBaseAngleDeg10;
  statusMsg += ",\"rightBaseAngleDeg10\":";
  statusMsg += rightBaseAngleDeg10;
  statusMsg += ",\"leftLight\":";
  statusMsg += left.lightOn ? "true" : "false";
  statusMsg += ",\"rightLight\":";
  statusMsg += right.lightOn ? "true" : "false";
  statusMsg += ",\"leftLightBreaker\":";
  statusMsg += left.lightBreakerActive ? "true" : "false";
  statusMsg += ",\"rightLightBreaker\":";
  statusMsg += right.lightBreakerActive ? "true" : "false";
  statusMsg += ",\"leftFog\":";
  statusMsg += left.fogOn ? "true" : "false";
  statusMsg += ",\"rightFog\":";
  statusMsg += right.fogOn ? "true" : "false";
  statusMsg += ",\"motorLink\":";
  statusMsg += motorLinkEnabled ? "true" : "false";
  statusMsg += ",\"motorMirror\":";
  statusMsg += motorMirrorEnabled ? "true" : "false";
  statusMsg += ",\"eyeballLink\":";
  statusMsg += eyeballLinkEnabled ? "true" : "false";
  statusMsg += ",\"eyeballMirror\":";
  statusMsg += eyeballMirrorEnabled ? "true" : "false";
  statusMsg += ",\"lightLink\":";
  statusMsg += lightLinkEnabled ? "true" : "false";
  statusMsg += ",\"fogLink\":";
  statusMsg += fogLinkEnabled ? "true" : "false";
  // Backward compatibility fields (left side mirrors legacy names)
  statusMsg += ",\"servoUpdown\":";
  statusMsg += left.servoUpdown;
  statusMsg += ",\"servoLateral\":";
  statusMsg += left.servoLateral;
  statusMsg += ",\"motorPwm\":";
  statusMsg += left.motorPwm;
  statusMsg += ",\"motorDir\":";
  statusMsg += left.motorDir;
  statusMsg += ",\"connectedNodes\":";
  statusMsg += countConnectedNodes();
  statusMsg += ",\"nodeCount\":";
  statusMsg += (NODE_COUNT + 1);
  statusMsg += ",\"nodeConnected\":";
  statusMsg += (countConnectedNodes() > 0) ? "true" : "false";
  statusMsg += ",\"nodes\":[";

  bool hostConnected = true;
  statusMsg += "{";
  statusMsg += "\"id\":0";
  statusMsg += ",\"role\":\"";
  statusMsg += th::toString(th::NodeRole::Coordinator);
  statusMsg += "\",\"label\":\"Base_L (Host)\"";
  statusMsg += ",\"connected\":";
  statusMsg += hostConnected ? "true" : "false";
  statusMsg += ",\"lastSeenMs\":";
  statusMsg += millis();
  statusMsg += ",\"testState\":\"";
  statusMsg += nodeTestStateToString(hostTestState);
  statusMsg += "\",\"testMessage\":\"";
  statusMsg += hostTestMessage;
  statusMsg += "\"},";

  for (uint8_t index = 0; index < NODE_COUNT; ++index) {
    NodeSlot &node = nodeSlots[index];
    statusMsg += "{";
    statusMsg += "\"id\":";
    statusMsg += node.id;
    statusMsg += ",\"role\":\"";
    statusMsg += th::toString(node.profile.role);
    statusMsg += "\",\"label\":\"";
    statusMsg += node.profile.label;
    statusMsg += "\",\"connected\":";
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

void sendMotorToRemoteBaseNodes(th::Side side, uint8_t pwmValue, uint8_t dirValue) {
  char tcpMsg[20];
  th::buildMotorCommand(pwmValue, dirValue, tcpMsg, sizeof(tcpMsg));

  for (uint8_t index = 0; index < NODE_COUNT; ++index) {
    NodeSlot &node = nodeSlots[index];
    if (!th::isBaseRole(node.profile.role) || !node.profile.capabilities.hasDcMotor || node.profile.side != side) {
      continue;
    }
    if (node.connected && node.client->connected()) {
      node.client->print(tcpMsg);
    }
  }
}

void sendServoToRemoteSatelliteNodes(th::Side side, int16_t updownValue, int16_t lateralValue) {
  char tcpMsg[32];
  th::buildServoCommand(updownValue, lateralValue, tcpMsg, sizeof(tcpMsg));

  for (uint8_t index = 0; index < NODE_COUNT; ++index) {
    NodeSlot &node = nodeSlots[index];
    if (!th::isSatelliteRole(node.profile.role) || !node.profile.capabilities.hasEyeballServos || node.profile.side != side) {
      continue;
    }
    if (node.connected && node.client->connected()) {
      node.client->print(tcpMsg);
    }
  }
}

void sendLightToRemoteSatelliteNodes(th::Side side, bool lightOn) {
  char tcpMsg[12];
  th::buildLightCommand(lightOn, tcpMsg, sizeof(tcpMsg));

  for (uint8_t index = 0; index < NODE_COUNT; ++index) {
    NodeSlot &node = nodeSlots[index];
    if (!th::isSatelliteRole(node.profile.role) || !node.profile.capabilities.hasEyeballServos || node.profile.side != side) {
      continue;
    }
    if (node.connected && node.client->connected()) {
      node.client->print(tcpMsg);
    }
  }
}

void sendLightBreakerToRemoteSatelliteNodes(th::Side side, bool breakerActive) {
  char tcpMsg[12];
  th::buildLightBreakerCommand(breakerActive, tcpMsg, sizeof(tcpMsg));

  for (uint8_t index = 0; index < NODE_COUNT; ++index) {
    NodeSlot &node = nodeSlots[index];
    if (!th::isSatelliteRole(node.profile.role) || !node.profile.capabilities.hasEyeballServos || node.profile.side != side) {
      continue;
    }
    if (node.connected && node.client->connected()) {
      node.client->print(tcpMsg);
    }
  }
}

void sendFogToRemoteSatelliteNodes(th::Side side, bool fogOn) {
  char tcpMsg[12];
  th::buildFogCommand(fogOn, tcpMsg, sizeof(tcpMsg));

  for (uint8_t index = 0; index < NODE_COUNT; ++index) {
    NodeSlot &node = nodeSlots[index];
    if (!th::isSatelliteRole(node.profile.role) || !node.profile.capabilities.hasEyeballServos || node.profile.side != side) {
      continue;
    }
    if (node.connected && node.client->connected()) {
      node.client->print(tcpMsg);
    }
  }
}

void sendNodeTestSignal(NodeSlot &node) {
  if (!node.connected || !node.client->connected()) {
    setNodeTestState(node, NODE_TEST_ERROR, "Node not connected");
    node.testPending = false;
    broadcastStatus();
    return;
  }

  node.client->println("TEST");
  node.lastTestMs = millis();
  node.testPending = true;
  setNodeTestState(node, NODE_TEST_SENDING, "Signal sent");
  broadcastStatus();
}

void updateNodeConnection(NodeSlot &node, bool connected, const char* message) {
  node.connected = connected;
  if (!connected) {
    node.client->stop();
    node.testPending = false;
    setNodeTestState(node, NODE_TEST_ERROR, message);
    if (th::isBaseRole(node.profile.role) && node.profile.side == th::Side::Right) {
      rightBaseAngleRaw = -1;
      rightBaseAngleDeg10 = -1;
    }
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
      node.client->setNoDelay(true);
      updateNodeConnection(node, true, "Node ready");
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
      updateNodeConnection(node, true, "Node ready");
      continue;
    }

    if (line.startsWith("PONG") || line.startsWith("TEST_OK")) {
      markNodeSeen(node);
      node.testPending = false;
      setNodeTestState(node, NODE_TEST_OK, "Reply from node");
      broadcastStatus();
      continue;
    }

    if (line.startsWith("HEARTBEAT") || line.startsWith("ALIVE")) {
      markNodeSeen(node);
      if (!node.connected) {
        updateNodeConnection(node, true, "Node ready");
      } else {
        broadcastStatus();
      }
      continue;
    }

    int angleRaw = -1;
    int angleDeg10 = -1;
    if (th::tryParseAngleReportCommand(line, &angleRaw, &angleDeg10)) {
      markNodeSeen(node);
      if (th::isBaseRole(node.profile.role) && node.profile.side == th::Side::Right) {
        int clampedRaw = constrain(angleRaw, 0, 4095);
        int clampedDeg10 = constrain(angleDeg10, 0, 3599);
        if (rightBaseAngleRaw != clampedRaw || rightBaseAngleDeg10 != clampedDeg10) {
          rightBaseAngleRaw = clampedRaw;
          rightBaseAngleDeg10 = clampedDeg10;
          broadcastStatus();
        }
      }
      continue;
    }
  }

  if (!node.client->connected()) {
    if (node.connected) {
      updateNodeConnection(node, false, "Connection lost");
      Serial.printf("[TCP] Node %u disconnected\n", node.id);
    }
  }
}

void checkNodeHealth(NodeSlot &node) {
  if (node.connected && node.lastSeenMs != 0 && (millis() - node.lastSeenMs) > NODE_HEARTBEAT_TIMEOUT_MS) {
    updateNodeConnection(node, false, "No heartbeat");
    Serial.printf("[TCP] Node %u heartbeat timeout\n", node.id);
    return;
  }

  if (node.testPending && (millis() - node.lastTestMs) > NODE_TEST_TIMEOUT_MS) {
    node.testPending = false;
    setNodeTestState(node, NODE_TEST_ERROR, "No reply");
    broadcastStatus();
  }
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
  SideControlState &left = getSideState(th::Side::Left);
  left.motorPwm = pwmValue;
  left.motorDir = dirValue ? 1 : 0;

  // Wahrheitstabelle:
  // PWM=Low -> Brake (DIR don't care)
  // PWM=High + DIR=Low -> CW
  // PWM=High + DIR=High -> CCW
  if (left.motorPwm == 0) {
    digitalWrite(MOTOR_DIR_PIN, LOW);
    analogWrite(MOTOR_PWM_PIN, 0);
    return;
  }

  digitalWrite(MOTOR_DIR_PIN, left.motorDir == 0 ? LOW : HIGH);
  analogWrite(MOTOR_PWM_PIN, left.motorPwm);
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

      if (strstr(buffer, "\"inputMode\":\"sequence\"") != nullptr) {
        currentInputMode = th::InputMode::Sequence;
      } else if (strstr(buffer, "\"inputMode\":\"manual\"") != nullptr) {
        currentInputMode = th::InputMode::Manual;
      }

      if (strstr(buffer, "\"nodeTest\"") != NULL) {
        int nodeId = -1;
        if (th::parseJsonIntInRange(buffer, "nodeId", 0, NODE_COUNT, &nodeId)) {
          if (nodeId == 0) {
            runHostDeviceTest();
            return;
          }
          NodeSlot* node = getNodeSlotById((uint8_t)nodeId);
          if (node != nullptr) {
            sendNodeTestSignal(*node);
          }
        }
        return;
      }

      SideControlState leftNext = getSideStateConst(th::Side::Left);
      SideControlState rightNext = getSideStateConst(th::Side::Right);
      bool leftServoChanged = false;
      bool rightServoChanged = false;
      bool leftMotorChanged = false;
      bool rightMotorChanged = false;
      bool leftLightChanged = false;
      bool rightLightChanged = false;
      bool leftLightBreakerChanged = false;
      bool rightLightBreakerChanged = false;
      bool leftFogChanged = false;
      bool rightFogChanged = false;
      bool stateChanged = false;
      th::Side servoSourceSide = th::Side::Unknown;
      th::Side motorSourceSide = th::Side::Unknown;
      th::Side lightSourceSide = th::Side::Unknown;
      th::Side lightBreakerSourceSide = th::Side::Unknown;
      th::Side fogSourceSide = th::Side::Unknown;

      bool parsedBoolValue = false;
      bool syncOptionChanged = false;
      if (th::parseJsonBool(buffer, "motorLink", &parsedBoolValue)) {
        motorLinkEnabled = parsedBoolValue;
        syncOptionChanged = true;
      }
      if (th::parseJsonBool(buffer, "motorMirror", &parsedBoolValue)) {
        motorMirrorEnabled = parsedBoolValue;
        syncOptionChanged = true;
      }
      if (th::parseJsonBool(buffer, "eyeballLink", &parsedBoolValue)) {
        eyeballLinkEnabled = parsedBoolValue;
        syncOptionChanged = true;
      }
      if (th::parseJsonBool(buffer, "eyeballMirror", &parsedBoolValue)) {
        eyeballMirrorEnabled = parsedBoolValue;
        syncOptionChanged = true;
      }
      if (th::parseJsonBool(buffer, "lightLink", &parsedBoolValue)) {
        lightLinkEnabled = parsedBoolValue;
        syncOptionChanged = true;
      }
      if (th::parseJsonBool(buffer, "fogLink", &parsedBoolValue)) {
        fogLinkEnabled = parsedBoolValue;
        syncOptionChanged = true;
      }
      if (th::parseJsonBool(buffer, "leftLight", &parsedBoolValue)) {
        leftNext.lightOn = parsedBoolValue;
        leftLightChanged = true;
        lightSourceSide = th::Side::Left;
      }
      if (th::parseJsonBool(buffer, "rightLight", &parsedBoolValue)) {
        rightNext.lightOn = parsedBoolValue;
        rightLightChanged = true;
        lightSourceSide = th::Side::Right;
      }
      if (th::parseJsonBool(buffer, "leftLightBreaker", &parsedBoolValue)) {
        leftNext.lightBreakerActive = parsedBoolValue;
        leftLightBreakerChanged = true;
        lightBreakerSourceSide = th::Side::Left;
      }
      if (th::parseJsonBool(buffer, "rightLightBreaker", &parsedBoolValue)) {
        rightNext.lightBreakerActive = parsedBoolValue;
        rightLightBreakerChanged = true;
        lightBreakerSourceSide = th::Side::Right;
      }
      if (th::parseJsonBool(buffer, "leftFog", &parsedBoolValue)) {
        leftNext.fogOn = parsedBoolValue;
        leftFogChanged = true;
        fogSourceSide = th::Side::Left;
      }
      if (th::parseJsonBool(buffer, "rightFog", &parsedBoolValue)) {
        rightNext.fogOn = parsedBoolValue;
        rightFogChanged = true;
        fogSourceSide = th::Side::Right;
      }

      if (!motorLinkEnabled) {
        motorMirrorEnabled = false;
      }
      if (!eyeballLinkEnabled) {
        eyeballMirrorEnabled = false;
      }

      const SideControlState &leftCurrentBeforeSync = getSideStateConst(th::Side::Left);
      const SideControlState &rightCurrentBeforeSync = getSideStateConst(th::Side::Right);
      const bool leftLightEffectivelyChanged = leftLightChanged && (leftNext.lightOn != leftCurrentBeforeSync.lightOn);
      const bool rightLightEffectivelyChanged = rightLightChanged && (rightNext.lightOn != rightCurrentBeforeSync.lightOn);
      if (leftLightEffectivelyChanged && !rightLightEffectivelyChanged) {
        lightSourceSide = th::Side::Left;
      } else if (rightLightEffectivelyChanged && !leftLightEffectivelyChanged) {
        lightSourceSide = th::Side::Right;
      }

      const bool leftLightBreakerEffectivelyChanged = leftLightBreakerChanged && (leftNext.lightBreakerActive != leftCurrentBeforeSync.lightBreakerActive);
      const bool rightLightBreakerEffectivelyChanged = rightLightBreakerChanged && (rightNext.lightBreakerActive != rightCurrentBeforeSync.lightBreakerActive);
      if (leftLightBreakerEffectivelyChanged && !rightLightBreakerEffectivelyChanged) {
        lightBreakerSourceSide = th::Side::Left;
      } else if (rightLightBreakerEffectivelyChanged && !leftLightBreakerEffectivelyChanged) {
        lightBreakerSourceSide = th::Side::Right;
      }

      const bool leftFogEffectivelyChanged = leftFogChanged && (leftNext.fogOn != leftCurrentBeforeSync.fogOn);
      const bool rightFogEffectivelyChanged = rightFogChanged && (rightNext.fogOn != rightCurrentBeforeSync.fogOn);
      if (leftFogEffectivelyChanged && !rightFogEffectivelyChanged) {
        fogSourceSide = th::Side::Left;
      } else if (rightFogEffectivelyChanged && !leftFogEffectivelyChanged) {
        fogSourceSide = th::Side::Right;
      }

      if (lightLinkEnabled && (leftLightChanged || rightLightChanged || syncOptionChanged)) {
        if (lightSourceSide == th::Side::Unknown) {
          lightSourceSide = th::Side::Left;
        }

        if (lightSourceSide == th::Side::Left) {
          rightNext.lightOn = leftNext.lightOn;
          rightLightChanged = true;
        } else {
          leftNext.lightOn = rightNext.lightOn;
          leftLightChanged = true;
        }
      }

      if (lightLinkEnabled && (leftLightBreakerChanged || rightLightBreakerChanged || syncOptionChanged)) {
        if (lightBreakerSourceSide == th::Side::Unknown) {
          lightBreakerSourceSide = th::Side::Left;
        }

        if (lightBreakerSourceSide == th::Side::Left) {
          rightNext.lightBreakerActive = leftNext.lightBreakerActive;
          rightLightBreakerChanged = true;
        } else {
          leftNext.lightBreakerActive = rightNext.lightBreakerActive;
          leftLightBreakerChanged = true;
        }
      }

      if (fogLinkEnabled && (leftFogChanged || rightFogChanged || syncOptionChanged)) {
        if (fogSourceSide == th::Side::Unknown) {
          fogSourceSide = th::Side::Left;
        }

        if (fogSourceSide == th::Side::Left) {
          rightNext.fogOn = leftNext.fogOn;
          rightFogChanged = true;
        } else {
          leftNext.fogOn = rightNext.fogOn;
          leftFogChanged = true;
        }
      }

      int tempValue = 0;
      if (th::parseJsonIntInRange(buffer, "leftServoUpdown", -updown_pos_limit, updown_pos_limit, &tempValue)) {
        leftNext.servoUpdown = (int16_t)tempValue;
        leftServoChanged = true;
        servoSourceSide = th::Side::Left;
      }
      if (th::parseJsonIntInRange(buffer, "leftServoLateral", -lateral_pos_limit, lateral_pos_limit, &tempValue)) {
        leftNext.servoLateral = (int16_t)tempValue;
        leftServoChanged = true;
        servoSourceSide = th::Side::Left;
      }
      if (th::parseJsonIntInRange(buffer, "rightServoUpdown", -updown_pos_limit, updown_pos_limit, &tempValue)) {
        rightNext.servoUpdown = (int16_t)tempValue;
        rightServoChanged = true;
        servoSourceSide = th::Side::Right;
      }
      if (th::parseJsonIntInRange(buffer, "rightServoLateral", -lateral_pos_limit, lateral_pos_limit, &tempValue)) {
        rightNext.servoLateral = (int16_t)tempValue;
        rightServoChanged = true;
        servoSourceSide = th::Side::Right;
      }

      // Legacy key fallback keeps old single-side clients functional.
      if (th::parseJsonIntInRange(buffer, "servoUpdown", -updown_pos_limit, updown_pos_limit, &tempValue)) {
        leftNext.servoUpdown = (int16_t)tempValue;
        leftServoChanged = true;
        servoSourceSide = th::Side::Left;
      }
      if (th::parseJsonIntInRange(buffer, "servoLateral", -lateral_pos_limit, lateral_pos_limit, &tempValue)) {
        leftNext.servoLateral = (int16_t)tempValue;
        leftServoChanged = true;
        servoSourceSide = th::Side::Left;
      }

      const SideControlState &leftCurrentBeforeServo = getSideStateConst(th::Side::Left);
      const SideControlState &rightCurrentBeforeServo = getSideStateConst(th::Side::Right);
      const bool leftServoEffectivelyChanged = leftServoChanged
        && (leftNext.servoUpdown != leftCurrentBeforeServo.servoUpdown || leftNext.servoLateral != leftCurrentBeforeServo.servoLateral);
      const bool rightServoEffectivelyChanged = rightServoChanged
        && (rightNext.servoUpdown != rightCurrentBeforeServo.servoUpdown || rightNext.servoLateral != rightCurrentBeforeServo.servoLateral);

      if (leftServoEffectivelyChanged && !rightServoEffectivelyChanged) {
        servoSourceSide = th::Side::Left;
      } else if (rightServoEffectivelyChanged && !leftServoEffectivelyChanged) {
        servoSourceSide = th::Side::Right;
      }

      if (eyeballLinkEnabled && (leftServoChanged || rightServoChanged || syncOptionChanged)) {
        if (servoSourceSide == th::Side::Unknown) {
          servoSourceSide = th::Side::Left;
        }

        if (servoSourceSide == th::Side::Left) {
          rightNext.servoUpdown = leftNext.servoUpdown;
          rightNext.servoLateral = eyeballMirrorEnabled ? mirrorEyeballLateral(leftNext.servoLateral) : leftNext.servoLateral;
          rightServoChanged = true;
        } else {
          leftNext.servoUpdown = rightNext.servoUpdown;
          leftNext.servoLateral = eyeballMirrorEnabled ? mirrorEyeballLateral(rightNext.servoLateral) : rightNext.servoLateral;
          leftServoChanged = true;
        }
      }

      if (leftServoChanged) {
        SideControlState &leftCurrent = getSideState(th::Side::Left);
        if (leftNext.servoUpdown != leftCurrent.servoUpdown) {
          Serial.printf("[SERVO LEFT UPDOWN] %d -> %d\n", leftCurrent.servoUpdown, leftNext.servoUpdown);
          applyServoFromUiValue(leftNext.servoUpdown, SERVO_UPDOWN_ID, leftCurrent.servoUpdown, updown_pos_limit);
        }
        if (leftNext.servoLateral != leftCurrent.servoLateral) {
          Serial.printf("[SERVO LEFT LATERAL] %d -> %d\n", leftCurrent.servoLateral, leftNext.servoLateral);
          applyServoFromUiValue(leftNext.servoLateral, SERVO_LATERAL_ID, leftCurrent.servoLateral, lateral_pos_limit);
        }
        sendServoToRemoteSatelliteNodes(th::Side::Left, leftCurrent.servoUpdown, leftCurrent.servoLateral);
        stateChanged = true;
      }

      if (rightServoChanged) {
        SideControlState &rightCurrent = getSideState(th::Side::Right);
        rightCurrent.servoUpdown = clampServoOffsetValue(rightNext.servoUpdown, updown_pos_limit);
        rightCurrent.servoLateral = clampServoOffsetValue(rightNext.servoLateral, lateral_pos_limit);
        sendServoToRemoteSatelliteNodes(th::Side::Right, rightCurrent.servoUpdown, rightCurrent.servoLateral);
        stateChanged = true;
      }

      if (leftLightChanged) {
        SideControlState &leftCurrent = getSideState(th::Side::Left);
        if (leftCurrent.lightOn != leftNext.lightOn) {
          leftCurrent.lightOn = leftNext.lightOn;
          Serial.printf("[LIGHT LEFT] %s\n", leftCurrent.lightOn ? "ON" : "OFF");
          sendLightToRemoteSatelliteNodes(th::Side::Left, leftCurrent.lightOn);
          stateChanged = true;
        }
      }

      if (rightLightChanged) {
        SideControlState &rightCurrent = getSideState(th::Side::Right);
        if (rightCurrent.lightOn != rightNext.lightOn) {
          rightCurrent.lightOn = rightNext.lightOn;
          Serial.printf("[LIGHT RIGHT] %s\n", rightCurrent.lightOn ? "ON" : "OFF");
          sendLightToRemoteSatelliteNodes(th::Side::Right, rightCurrent.lightOn);
          stateChanged = true;
        }
      }

      if (leftLightBreakerChanged) {
        SideControlState &leftCurrent = getSideState(th::Side::Left);
        if (leftCurrent.lightBreakerActive != leftNext.lightBreakerActive) {
          leftCurrent.lightBreakerActive = leftNext.lightBreakerActive;
          Serial.printf("[LIGHT BREAKER LEFT] %s\n", leftCurrent.lightBreakerActive ? "ACTIVE" : "RELEASED");
          sendLightBreakerToRemoteSatelliteNodes(th::Side::Left, leftCurrent.lightBreakerActive);
          stateChanged = true;
        }
      }

      if (rightLightBreakerChanged) {
        SideControlState &rightCurrent = getSideState(th::Side::Right);
        if (rightCurrent.lightBreakerActive != rightNext.lightBreakerActive) {
          rightCurrent.lightBreakerActive = rightNext.lightBreakerActive;
          Serial.printf("[LIGHT BREAKER RIGHT] %s\n", rightCurrent.lightBreakerActive ? "ACTIVE" : "RELEASED");
          sendLightBreakerToRemoteSatelliteNodes(th::Side::Right, rightCurrent.lightBreakerActive);
          stateChanged = true;
        }
      }

      if (leftFogChanged) {
        SideControlState &leftCurrent = getSideState(th::Side::Left);
        if (leftCurrent.fogOn != leftNext.fogOn) {
          leftCurrent.fogOn = leftNext.fogOn;
          Serial.printf("[FOG LEFT] %s\n", leftCurrent.fogOn ? "ON" : "OFF");
          sendFogToRemoteSatelliteNodes(th::Side::Left, leftCurrent.fogOn);
          stateChanged = true;
        }
      }

      if (rightFogChanged) {
        SideControlState &rightCurrent = getSideState(th::Side::Right);
        if (rightCurrent.fogOn != rightNext.fogOn) {
          rightCurrent.fogOn = rightNext.fogOn;
          Serial.printf("[FOG RIGHT] %s\n", rightCurrent.fogOn ? "ON" : "OFF");
          sendFogToRemoteSatelliteNodes(th::Side::Right, rightCurrent.fogOn);
          stateChanged = true;
        }
      }

      int newLeftMotorPwm = getSideStateConst(th::Side::Left).motorPwm;
      int newLeftMotorDir = getSideStateConst(th::Side::Left).motorDir;
      int newRightMotorPwm = getSideStateConst(th::Side::Right).motorPwm;
      int newRightMotorDir = getSideStateConst(th::Side::Right).motorDir;
      int parsedMotorPwm = -1;
      int parsedMotorDir = -1;

      if (th::parseJsonIntInRange(buffer, "leftMotorPwm", 0, 255, &parsedMotorPwm)) {
        newLeftMotorPwm = parsedMotorPwm;
        leftMotorChanged = true;
        motorSourceSide = th::Side::Left;
      }
      if (th::parseJsonIntInRange(buffer, "leftMotorDir", 0, 1, &parsedMotorDir)) {
        newLeftMotorDir = parsedMotorDir;
        leftMotorChanged = true;
        motorSourceSide = th::Side::Left;
      }
      if (th::parseJsonIntInRange(buffer, "rightMotorPwm", 0, 255, &parsedMotorPwm)) {
        newRightMotorPwm = parsedMotorPwm;
        rightMotorChanged = true;
        motorSourceSide = th::Side::Right;
      }
      if (th::parseJsonIntInRange(buffer, "rightMotorDir", 0, 1, &parsedMotorDir)) {
        newRightMotorDir = parsedMotorDir;
        rightMotorChanged = true;
        motorSourceSide = th::Side::Right;
      }

      if (th::parseJsonIntInRange(buffer, "motorPwm", 0, 255, &parsedMotorPwm)) {
        newLeftMotorPwm = parsedMotorPwm;
        leftMotorChanged = true;
        motorSourceSide = th::Side::Left;
      }
      if (th::parseJsonIntInRange(buffer, "motorDir", 0, 1, &parsedMotorDir)) {
        newLeftMotorDir = parsedMotorDir;
        leftMotorChanged = true;
        motorSourceSide = th::Side::Left;
      }

      const SideControlState &leftCurrentBeforeMotor = getSideStateConst(th::Side::Left);
      const SideControlState &rightCurrentBeforeMotor = getSideStateConst(th::Side::Right);
      const bool leftMotorEffectivelyChanged = leftMotorChanged
        && ((uint8_t)newLeftMotorPwm != leftCurrentBeforeMotor.motorPwm || (uint8_t)newLeftMotorDir != leftCurrentBeforeMotor.motorDir);
      const bool rightMotorEffectivelyChanged = rightMotorChanged
        && ((uint8_t)newRightMotorPwm != rightCurrentBeforeMotor.motorPwm || (uint8_t)newRightMotorDir != rightCurrentBeforeMotor.motorDir);

      if (leftMotorEffectivelyChanged && !rightMotorEffectivelyChanged) {
        motorSourceSide = th::Side::Left;
      } else if (rightMotorEffectivelyChanged && !leftMotorEffectivelyChanged) {
        motorSourceSide = th::Side::Right;
      }

      if (motorLinkEnabled && (leftMotorChanged || rightMotorChanged || syncOptionChanged)) {
        if (motorSourceSide == th::Side::Unknown) {
          motorSourceSide = th::Side::Left;
        }

        if (motorSourceSide == th::Side::Left) {
          newRightMotorPwm = newLeftMotorPwm;
          newRightMotorDir = motorMirrorEnabled ? mirroredMotorDir((uint8_t)newLeftMotorDir) : newLeftMotorDir;
          rightMotorChanged = true;
        } else {
          newLeftMotorPwm = newRightMotorPwm;
          newLeftMotorDir = motorMirrorEnabled ? mirroredMotorDir((uint8_t)newRightMotorDir) : newRightMotorDir;
          leftMotorChanged = true;
        }
      }

      if (leftMotorChanged) {
        const SideControlState &leftBefore = getSideStateConst(th::Side::Left);
        if ((uint8_t)newLeftMotorPwm != leftBefore.motorPwm || (uint8_t)newLeftMotorDir != leftBefore.motorDir) {
          Serial.printf("[MOTOR LEFT] PWM=%d DIR=%d\n", newLeftMotorPwm, newLeftMotorDir);
        }
        applyDcMotorDriver((uint8_t)newLeftMotorPwm, (uint8_t)newLeftMotorDir);
        sendMotorToRemoteBaseNodes(th::Side::Left, getSideStateConst(th::Side::Left).motorPwm, getSideStateConst(th::Side::Left).motorDir);
        stateChanged = true;
      }

      if (rightMotorChanged) {
        SideControlState &rightCurrent = getSideState(th::Side::Right);
        if ((uint8_t)newRightMotorPwm != rightCurrent.motorPwm || (uint8_t)newRightMotorDir != rightCurrent.motorDir) {
          Serial.printf("[MOTOR RIGHT] PWM=%d DIR=%d\n", newRightMotorPwm, newRightMotorDir);
        }
        rightCurrent.motorPwm = (uint8_t)newRightMotorPwm;
        rightCurrent.motorDir = (uint8_t)newRightMotorDir;
        sendMotorToRemoteBaseNodes(th::Side::Right, rightCurrent.motorPwm, rightCurrent.motorDir);
        stateChanged = true;
      }

      if (stateChanged || syncOptionChanged) {
        broadcastStatus();
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

  Wire.begin(AS5600_SDA_PIN, AS5600_SCL_PIN);
  Serial.printf("[AS5600] I2C initialized on SDA=%u SCL=%u\n", AS5600_SDA_PIN, AS5600_SCL_PIN);

  // UART-Servo (SC09) initialisieren: TX=GPIO21, RX=GPIO20
  Serial1.begin(SERVO_BAUD, SERIAL_8N1, SERVO_UART_RX_PIN, SERVO_UART_TX_PIN);
  scServo.pSerial = &Serial1;
  delay(100);
  SideControlState &left = getSideState(th::Side::Left);
  applyServoFromUiValue(left.servoUpdown, SERVO_UPDOWN_ID, left.servoUpdown, updown_pos_limit);
  applyServoFromUiValue(left.servoLateral, SERVO_LATERAL_ID, left.servoLateral, lateral_pos_limit);
  // Start servo task to handle actual SCServo writes outside network/event context
  xTaskCreatePinnedToCore(servoTask, "servoTask", 4096, NULL, 1, NULL, 0);
  Serial.printf("[SERVO] Updown ID %u, Lateral ID %u\n", SERVO_UPDOWN_ID, SERVO_LATERAL_ID);

  // WiFi Access Point starten
  th::startAccessPoint(WIFI_SSID, WIFI_PASS);

  // WebSocket initialisieren
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  // HTTP Endpoints
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", th::getWebPageHtml());
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
  pollLocalBaseAs5600();

  for (uint8_t index = 0; index < NODE_COUNT; ++index) {
    serviceNodeSlot(nodeSlots[index]);
    checkNodeHealth(nodeSlots[index]);
  }

  delay(2);
}

