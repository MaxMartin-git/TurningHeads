#pragma once

namespace th {

inline const char* getWebPageHtml() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>TurningHeads</title>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <style>
    html, body { height: 100%; }
    * {
      -webkit-touch-callout: none;
      -webkit-user-select: none;
      user-select: none;
      -webkit-tap-highlight-color: transparent;
    }
    body {
      font-family: "Segoe UI", sans-serif;
      margin: 0;
      background: radial-gradient(circle at top, #2a2a2a 0%, #1b1b1b 60%, #131313 100%);
      color: #fff;
      overscroll-behavior: none;
    }
    .layout {
      max-width: 760px;
      margin: 12px auto;
      padding: 10px;
      box-sizing: border-box;
    }
    h1 {
      margin: 0 0 10px;
      text-align: center;
      font-size: 20px;
      letter-spacing: 1px;
      color: #9de79f;
    }
    .main-grid {
      display: grid;
      grid-template-columns: 1fr 180px 1fr;
      gap: 8px;
    }
    .side-card {
      background: #252525;
      border: 1px solid #3f3f3f;
      border-radius: 10px;
      padding: 8px;
      box-shadow: 0 8px 24px rgba(0, 0, 0, 0.25);
    }
    .side-title {
      text-align: center;
      font-size: 13px;
      font-weight: bold;
      margin-bottom: 6px;
      color: #d9ffd9;
    }
    .sync-card {
      background: #202020;
      border: 1px solid #3f3f3f;
      border-radius: 10px;
      padding: 10px 8px;
      display: flex;
      flex-direction: column;
      gap: 10px;
      align-self: start;
    }
    .sync-title {
      text-align: center;
      font-size: 12px;
      color: #bfe9c0;
      font-weight: bold;
    }
    .sync-group {
      background: #262626;
      border: 1px solid #3c3c3c;
      border-radius: 8px;
      padding: 7px;
      font-size: 11px;
    }
    .sync-group-head {
      font-weight: bold;
      margin-bottom: 4px;
      color: #e2fbe3;
      text-align: center;
    }
    .sync-check {
      display: flex;
      align-items: center;
      gap: 6px;
      margin: 4px 0;
      cursor: pointer;
      user-select: none;
    }
    .sync-check input {
      accent-color: #63d767;
    }
    .joystick-panel {
      margin: 0;
    }
    .joystick-readout {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 6px;
      margin-bottom: 6px;
      font-weight: bold;
      font-size: 11px;
    }
    .joystick-readout div {
      background: #1f1f1f;
      border: 1px solid #4b4b4b;
      border-radius: 6px;
      padding: 6px;
      text-align: center;
    }
    .joystick-readout span {
      display: inline-block;
      min-width: 4ch;
      font-size: 10px;
      font-family: "Consolas", "Courier New", monospace;
      font-variant-numeric: tabular-nums;
    }
    .joystick-pad {
      position: relative;
      width: min(42vw, 170px);
      aspect-ratio: 1 / 1;
      margin: 0 auto;
      border-radius: 12px;
      border: 1px solid #555;
      background:
        radial-gradient(circle at center, rgba(76, 175, 80, 0.22), transparent 45%),
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
      width: 34px;
      height: 34px;
      margin-left: -17px;
      margin-top: -17px;
      border-radius: 50%;
      background: radial-gradient(circle at 30% 30%, #a7f7a9, #39b84c 55%, #23762f 100%);
      box-shadow: 0 6px 16px rgba(0, 0, 0, 0.35);
      transform: translate(0px, 0px);
      transition: transform 40ms linear;
      pointer-events: none;
    }
    .mini-label {
      margin-top: 6px;
      color: #bdbdbd;
      font-size: 11px;
      text-align: center;
    }
    .angle-panel {
      margin-top: 8px;
      display: grid;
      grid-template-columns: 56px 90px;
      align-items: center;
      justify-content: center;
      gap: 10px;
      background: #1d1d1d;
      border: 1px solid #3f3f3f;
      border-radius: 8px;
      padding: 7px;
    }
    .angle-gauge {
      width: 56px;
      height: 56px;
      border-radius: 50%;
      border: 2px solid #5d5d5d;
      position: relative;
      background: radial-gradient(circle at center, #2f2f2f 0%, #202020 70%, #1a1a1a 100%);
      box-shadow: inset 0 0 12px rgba(0, 0, 0, 0.5);
    }
    .angle-needle {
      position: absolute;
      left: 50%;
      top: 50%;
      width: 2px;
      height: 23px;
      margin-left: -1px;
      margin-top: -23px;
      border-radius: 999px;
      background: #ff5f45;
      transform-origin: 50% 100%;
      transform: rotate(0deg);
      box-shadow: 0 0 8px rgba(255, 95, 69, 0.8);
      transition: transform 80ms linear;
    }
    .angle-center-dot {
      position: absolute;
      width: 8px;
      height: 8px;
      border-radius: 50%;
      left: 50%;
      top: 50%;
      margin-left: -4px;
      margin-top: -4px;
      background: #d7d7d7;
      border: 1px solid #3a3a3a;
    }
    .angle-text {
      display: flex;
      flex-direction: column;
      align-items: flex-start;
      line-height: 1.15;
      min-width: 72px;
    }
    .angle-text .label {
      font-size: 10px;
      color: #a9a9a9;
      letter-spacing: 0.5px;
      text-transform: uppercase;
    }
    .angle-text .value {
      font-family: "Consolas", "Courier New", monospace;
      font-size: 18px;
      color: #f0fff0;
      font-variant-numeric: tabular-nums;
      width: 100%;
      text-align: right;
      white-space: nowrap;
    }
    .angle-number {
      display: inline-block;
      width: 5ch;
      text-align: right;
      font-variant-numeric: tabular-nums;
    }
    .angle-unit {
      display: inline-block;
      width: 2ch;
      text-align: left;
    }
    .motor-row {
      margin-top: 6px;
      display: grid;
      grid-template-columns: 1fr auto;
      align-items: center;
      gap: 6px;
      font-size: 12px;
    }
    .velocity-title {
      margin-top: 6px;
      font-size: 11px;
      color: #d7d7d7;
      text-align: center;
      letter-spacing: 0.4px;
      text-transform: uppercase;
    }
    .rotation-slider {
      width: 100%;
      margin-top: 4px;
      height: 22px;
      -webkit-appearance: none;
      appearance: none;
      background: #5a5a5a;
      border-radius: 999px;
      outline: none;
    }
    .rotation-slider::-webkit-slider-runnable-track {
      height: 12px;
      border-radius: 999px;
      background: transparent;
    }
    .rotation-slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 20px;
      height: 20px;
      margin-top: -4px;
      border-radius: 50%;
      border: 1px solid rgba(255,255,255,0.5);
      background: #f1fff1;
      box-shadow: 0 2px 8px rgba(0, 0, 0, 0.45);
      cursor: pointer;
    }
    .rotation-slider::-moz-range-track {
      height: 12px;
      border-radius: 999px;
      background: transparent;
    }
    .rotation-slider::-moz-range-thumb {
      width: 20px;
      height: 20px;
      border-radius: 50%;
      border: 1px solid rgba(255,255,255,0.5);
      background: #f1fff1;
      box-shadow: 0 2px 8px rgba(0, 0, 0, 0.45);
      cursor: pointer;
    }
    .stop-row {
      margin-top: 6px;
      display: flex;
      justify-content: center;
    }
    .light-row {
      margin-top: 8px;
      display: flex;
      justify-content: center;
      align-items: center;
      gap: 8px;
      font-size: 11px;
      color: #d7d7d7;
    }
    .momentary-btn {
      border: 1px solid rgba(255,255,255,0.25);
      border-radius: 6px;
      padding: 6px 8px;
      font-weight: bold;
      font-size: 11px;
      background: #4b4b4b;
      color: #f2f2f2;
      min-width: 68px;
    }
    .momentary-btn.active {
      background: #59cb5e;
      border-color: #8be98d;
      color: #102010;
      box-shadow: 0 0 12px rgba(89, 203, 94, 0.6);
    }
    .switch {
      position: relative;
      display: inline-block;
      width: 42px;
      height: 22px;
    }
    .switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }
    .switch-slider {
      position: absolute;
      cursor: pointer;
      inset: 0;
      background-color: #5e5e5e;
      transition: 0.2s;
      border-radius: 22px;
    }
    .switch-slider:before {
      position: absolute;
      content: "";
      height: 18px;
      width: 18px;
      left: 2px;
      top: 2px;
      background-color: #f2fff2;
      transition: 0.2s;
      border-radius: 50%;
    }
    .switch input:checked + .switch-slider {
      background-color: #59cb5e;
    }
    .switch input:checked + .switch-slider:before {
      transform: translateX(20px);
    }
    .btn {
      border: 0;
      border-radius: 6px;
      padding: 7px 8px;
      font-weight: bold;
      font-size: 12px;
      background: #4CAF50;
      color: #111;
    }
    .btn:active {
      transform: translateY(1px);
    }
    .node-grid {
      margin-top: 10px;
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(132px, 1fr));
      gap: 8px;
    }
    .node-card {
      border: 1px solid rgba(255,255,255,0.08);
      border-radius: 8px;
      padding: 8px;
      background: #2b2b2b;
      text-align: left;
      display: flex;
      flex-direction: column;
      gap: 6px;
      font-size: 11px;
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
      font-size: 11px;
      min-height: 14px;
    }
    .signal-light {
      width: 12px;
      height: 12px;
      border-radius: 50%;
      background: #666;
      box-shadow: 0 0 0 3px rgba(255,255,255,0.06);
    }
    .signal-idle { background: #666; }
    .signal-sending { background: #ffb300; box-shadow: 0 0 14px rgba(255, 179, 0, 0.7); }
    .signal-ok { background: #4CAF50; box-shadow: 0 0 14px rgba(76, 175, 80, 0.8); }
    .signal-error { background: #f44336; box-shadow: 0 0 14px rgba(244, 67, 54, 0.75); }
    #status {
      margin-top: 10px;
      padding: 8px;
      border-radius: 6px;
      background: #444;
      font-size: 12px;
      text-align: center;
    }
    .status-ok { color: #4CAF50; }
    .status-error { color: #f44336; }
    @media (max-width: 620px) {
      .main-grid {
        grid-template-columns: 1fr;
      }
      .joystick-pad {
        width: min(62vw, 180px);
      }
    }
  </style>
</head>
<body>
  <div class="layout">
    <h1>TurningHeads</h1>

    <div class="main-grid">
      <div class="side-card" data-side="left">
        <div class="side-title">LEFT SIDE</div>
        <div class="joystick-panel">
          <div class="joystick-readout">
            <div>Up: <span id="leftServoUpdownValue">0</span></div>
            <div>Lat: <span id="leftServoLateralValue">0</span></div>
          </div>
          <div id="leftJoystickPad" class="joystick-pad" aria-label="Left servo joystick">
            <div id="leftJoystickThumb" class="joystick-thumb"></div>
          </div>
          <div class="mini-label">Left eyeball axis</div>
          <div class="angle-panel">
            <div class="angle-gauge" aria-label="Left base angle">
              <div id="leftBaseAngleNeedle" class="angle-needle"></div>
              <div class="angle-center-dot"></div>
            </div>
            <div class="angle-text">
              <div class="label">Base Angle</div>
              <div class="value"><span id="leftBaseAngleValue" class="angle-number">--.-</span><span class="angle-unit">deg</span></div>
            </div>
          </div>
        </div>
        <div class="motor-row">
          <label for="leftMotorSlider">Head Rotation</label>
          <span id="leftMotorValue">0</span>
        </div>
        <div class="velocity-title">Velocity</div>
        <input class="rotation-slider" type="range" id="leftMotorSlider" min="-255" max="255" value="0">
        <div class="stop-row">
          <button id="leftStopButton" class="btn" type="button">Stop</button>
        </div>
        <div class="light-row">
          <span>Light</span>
          <label class="switch" for="leftLightSwitch">
            <input type="checkbox" id="leftLightSwitch">
            <span class="switch-slider"></span>
          </label>
          <button id="leftLightBreakerButton" class="momentary-btn" type="button">Breaker</button>
        </div>
        <div class="light-row">
          <span>Fog</span>
          <button id="leftFogButton" class="momentary-btn" type="button">Fog</button>
        </div>
      </div>

      <div class="sync-card">
        <div class="sync-title">LINK</div>
        <div class="sync-group">
          <div class="sync-group-head">Eyeball</div>
          <label class="sync-check" for="eyeballLink">
            <input type="checkbox" id="eyeballLink" checked>
            <span>Link L/R</span>
          </label>
          <label class="sync-check" for="eyeballMirror">
            <input type="checkbox" id="eyeballMirror" checked>
            <span>Mirror</span>
          </label>
        </div>
        <div class="sync-group">
          <div class="sync-group-head">Head Rotation</div>
          <label class="sync-check" for="motorLink">
            <input type="checkbox" id="motorLink" checked>
            <span>Link L/R</span>
          </label>
          <label class="sync-check" for="motorMirror">
            <input type="checkbox" id="motorMirror" checked>
            <span>Mirror</span>
          </label>
        </div>
        <div class="sync-group">
          <div class="sync-group-head">Light</div>
          <label class="sync-check" for="lightLink">
            <input type="checkbox" id="lightLink" checked>
            <span>Link L/R</span>
          </label>
        </div>
        <div class="sync-group">
          <div class="sync-group-head">Fog</div>
          <label class="sync-check" for="fogLink">
            <input type="checkbox" id="fogLink" checked>
            <span>Link L/R</span>
          </label>
        </div>
      </div>

      <div class="side-card" data-side="right">
        <div class="side-title">RIGHT SIDE</div>
        <div class="joystick-panel">
          <div class="joystick-readout">
            <div>Up: <span id="rightServoUpdownValue">0</span></div>
            <div>Lat: <span id="rightServoLateralValue">0</span></div>
          </div>
          <div id="rightJoystickPad" class="joystick-pad" aria-label="Right servo joystick">
            <div id="rightJoystickThumb" class="joystick-thumb"></div>
          </div>
          <div class="mini-label">Right eyeball axis</div>
          <div class="angle-panel">
            <div class="angle-gauge" aria-label="Right base angle">
              <div id="rightBaseAngleNeedle" class="angle-needle"></div>
              <div class="angle-center-dot"></div>
            </div>
            <div class="angle-text">
              <div class="label">Base Angle</div>
              <div class="value"><span id="rightBaseAngleValue" class="angle-number">--.-</span><span class="angle-unit">deg</span></div>
            </div>
          </div>
        </div>
        <div class="motor-row">
          <label for="rightMotorSlider">Head Rotation</label>
          <span id="rightMotorValue">0</span>
        </div>
        <div class="velocity-title">Velocity</div>
        <input class="rotation-slider" type="range" id="rightMotorSlider" min="-255" max="255" value="0">
        <div class="stop-row">
          <button id="rightStopButton" class="btn" type="button">Stop</button>
        </div>
        <div class="light-row">
          <span>Light</span>
          <label class="switch" for="rightLightSwitch">
            <input type="checkbox" id="rightLightSwitch">
            <span class="switch-slider"></span>
          </label>
          <button id="rightLightBreakerButton" class="momentary-btn" type="button">Breaker</button>
        </div>
        <div class="light-row">
          <span>Fog</span>
          <button id="rightFogButton" class="momentary-btn" type="button">Fog</button>
        </div>
      </div>
    </div>

    <div class="node-grid">
      <div class="node-card">
        <div id="node0Header" class="node-card-header">Base_L (Host)</div>
        <div class="node-card-status">
          <span id="node0Light" class="signal-light signal-idle"></span>
          <span id="node0Connection">Disconnected</span>
        </div>
        <button id="node0TestButton" class="btn" type="button">Send Test Signal</button>
        <div id="node0Message" class="node-card-message">Ready</div>
      </div>
      <div class="node-card">
        <div id="node1Header" class="node-card-header">Satellite_L</div>
        <div class="node-card-status">
          <span id="node1Light" class="signal-light signal-idle"></span>
          <span id="node1Connection">Disconnected</span>
        </div>
        <button id="node1TestButton" class="btn" type="button">Send Test Signal</button>
        <div id="node1Message" class="node-card-message">Ready</div>
      </div>
      <div class="node-card">
        <div id="node2Header" class="node-card-header">Base_R</div>
        <div class="node-card-status">
          <span id="node2Light" class="signal-light signal-idle"></span>
          <span id="node2Connection">Disconnected</span>
        </div>
        <button id="node2TestButton" class="btn" type="button">Send Test Signal</button>
        <div id="node2Message" class="node-card-message">Ready</div>
      </div>
      <div class="node-card">
        <div id="node3Header" class="node-card-header">Satellite_R</div>
        <div class="node-card-status">
          <span id="node3Light" class="signal-light signal-idle"></span>
          <span id="node3Connection">Disconnected</span>
        </div>
        <button id="node3TestButton" class="btn" type="button">Send Test Signal</button>
        <div id="node3Message" class="node-card-message">Ready</div>
      </div>
    </div>

    <div id="status">
      <div>WebSocket: <span id="wsStatus" class="status-error">Disconnected</span></div>
      <div>Node: <span id="nodeStatus" class="status-error">No signal</span></div>
    </div>
  </div>

  <script>
    const ws = new WebSocket('ws://' + window.location.host + '/ws');
    const wsStatus = document.getElementById('wsStatus');
    const nodeStatus = document.getElementById('nodeStatus');
    const UPDOWN_LIMIT = 140;
    const LATERAL_LIMIT = 160;
    const HEAD_ROTATION_AXIS_MAX = 255;
    const HEAD_ROTATION_DEADZONE = 16;
    let statusSynced = false;
    let controlStateDirty = false;
    const syncOptions = {
      motorLink: true,
      motorMirror: true,
      eyeballLink: true,
      eyeballMirror: true,
      lightLink: true,
      fogLink: true
    };
    const syncUi = {
      motorLink: document.getElementById('motorLink'),
      motorMirror: document.getElementById('motorMirror'),
      eyeballLink: document.getElementById('eyeballLink'),
      eyeballMirror: document.getElementById('eyeballMirror'),
      lightLink: document.getElementById('lightLink'),
      fogLink: document.getElementById('fogLink')
    };
    const sides = {
      left: {
        servoUpdown: 0,
        servoLateral: 0,
        motorAxis: 0,
        motorPwm: 0,
        motorDir: 0,
        lightOn: false,
        lightBreakerActive: false,
        fogOn: false,
        pad: document.getElementById('leftJoystickPad'),
        thumb: document.getElementById('leftJoystickThumb'),
        updownText: document.getElementById('leftServoUpdownValue'),
        lateralText: document.getElementById('leftServoLateralValue'),
        motorSlider: document.getElementById('leftMotorSlider'),
        motorValue: document.getElementById('leftMotorValue'),
        stopButton: document.getElementById('leftStopButton'),
        lightSwitch: document.getElementById('leftLightSwitch'),
        lightBreakerButton: document.getElementById('leftLightBreakerButton'),
        fogButton: document.getElementById('leftFogButton'),
        baseAngleDeg10: -1,
        baseAngleVisualDeg: null,
        angleNeedle: document.getElementById('leftBaseAngleNeedle'),
        angleValue: document.getElementById('leftBaseAngleValue'),
        dragging: false
      },
      right: {
        servoUpdown: 0,
        servoLateral: 0,
        motorAxis: 0,
        motorPwm: 0,
        motorDir: 0,
        lightOn: false,
        lightBreakerActive: false,
        fogOn: false,
        pad: document.getElementById('rightJoystickPad'),
        thumb: document.getElementById('rightJoystickThumb'),
        updownText: document.getElementById('rightServoUpdownValue'),
        lateralText: document.getElementById('rightServoLateralValue'),
        motorSlider: document.getElementById('rightMotorSlider'),
        motorValue: document.getElementById('rightMotorValue'),
        stopButton: document.getElementById('rightStopButton'),
        lightSwitch: document.getElementById('rightLightSwitch'),
        lightBreakerButton: document.getElementById('rightLightBreakerButton'),
        fogButton: document.getElementById('rightFogButton'),
        baseAngleDeg10: -1,
        baseAngleVisualDeg: null,
        angleNeedle: document.getElementById('rightBaseAngleNeedle'),
        angleValue: document.getElementById('rightBaseAngleValue'),
        dragging: false
      }
    };
    const nodeUi = [0, 1, 2, 3].map(function(nodeId) {
      return {
        id: nodeId,
        header: document.getElementById('node' + nodeId + 'Header'),
        button: document.getElementById('node' + nodeId + 'TestButton'),
        light: document.getElementById('node' + nodeId + 'Light'),
        connection: document.getElementById('node' + nodeId + 'Connection'),
        message: document.getElementById('node' + nodeId + 'Message')
      };
    }).filter(function(entry) {
      return !!entry && !!entry.header && !!entry.button && !!entry.light && !!entry.connection && !!entry.message;
    });

    function clamp(value, min, max) {
      return Math.min(max, Math.max(min, value));
    }

    function syncSideReadout(side) {
      side.updownText.textContent = side.servoUpdown;
      side.lateralText.textContent = side.servoLateral;
      if (side.motorPwm === 0) {
        side.motorValue.textContent = 'STOP';
      } else {
        side.motorValue.textContent = (side.motorDir === 1 ? 'CCW ' : 'CW ') + side.motorPwm;
      }
      side.motorSlider.value = side.motorAxis;
      updateRotationSliderVisual(side);
      side.lightSwitch.checked = !!side.lightOn;
      side.lightBreakerButton.classList.toggle('active', !!side.lightBreakerActive);
      side.fogButton.classList.toggle('active', !!side.fogOn);
      renderBaseAngle(side);
    }

    function renderBaseAngle(side) {
      const hasAngle = typeof side.baseAngleDeg10 === 'number' && side.baseAngleDeg10 >= 0;
      if (!hasAngle) {
        side.angleValue.textContent = '--.-';
        side.baseAngleVisualDeg = null;
        side.angleNeedle.style.transform = 'rotate(0deg)';
        side.angleNeedle.style.opacity = '0.35';
        return;
      }

      const normalized = ((side.baseAngleDeg10 % 3600) + 3600) % 3600;
      const degrees = normalized / 10;
      side.angleValue.textContent = degrees.toFixed(1);

      if (typeof side.baseAngleVisualDeg !== 'number') {
        side.baseAngleVisualDeg = degrees;
      } else {
        let delta = degrees - side.baseAngleVisualDeg;
        while (delta > 180) {
          delta -= 360;
        }
        while (delta < -180) {
          delta += 360;
        }
        side.baseAngleVisualDeg += delta;
      }

      const visualDegrees = side.baseAngleVisualDeg + 180;
      side.angleNeedle.style.transform = 'rotate(' + visualDegrees.toFixed(1) + 'deg)';
      side.angleNeedle.style.opacity = '1';
    }

    function updateRotationSliderVisual(side) {
      const axis = clamp(parseInt(side.motorAxis, 10) || 0, -HEAD_ROTATION_AXIS_MAX, HEAD_ROTATION_AXIS_MAX);
      const centerPct = 50;
      const axisPct = centerPct + (axis / HEAD_ROTATION_AXIS_MAX) * 50;
      let gradient;

      if (axis >= 0) {
        gradient = 'linear-gradient(90deg, #5a5a5a 0%, #5a5a5a ' + centerPct + '%, #59cb5e ' + centerPct + '%, #59cb5e ' + axisPct + '%, #5a5a5a ' + axisPct + '%, #5a5a5a 100%)';
      } else {
        gradient = 'linear-gradient(90deg, #5a5a5a 0%, #5a5a5a ' + axisPct + '%, #59cb5e ' + axisPct + '%, #59cb5e ' + centerPct + '%, #5a5a5a ' + centerPct + '%, #5a5a5a 100%)';
      }

      side.motorSlider.style.background = gradient;
    }

    function axisToMotorCommand(axisValue) {
      const axis = clamp(parseInt(axisValue, 10) || 0, -HEAD_ROTATION_AXIS_MAX, HEAD_ROTATION_AXIS_MAX);
      if (Math.abs(axis) <= HEAD_ROTATION_DEADZONE) {
        return {axis: 0, pwm: 0, dir: 0};
      }

      return {
        axis: axis,
        pwm: Math.abs(axis),
        dir: axis > 0 ? 1 : 0
      };
    }

    function motorCommandToAxis(pwmValue, dirValue) {
      const pwm = clamp(parseInt(pwmValue, 10) || 0, 0, HEAD_ROTATION_AXIS_MAX);
      if (pwm === 0) {
        return 0;
      }
      return (dirValue === 1 ? 1 : -1) * pwm;
    }

    function setHeadRotationAxis(side, axisValue) {
      const cmd = axisToMotorCommand(axisValue);
      side.motorAxis = cmd.axis;
      side.motorPwm = cmd.pwm;
      side.motorDir = cmd.dir;
      syncSideReadout(side);
    }

    function renderJoystickThumb(side) {
      const rect = side.pad.getBoundingClientRect();
      const thumbSize = side.thumb.offsetWidth || 34;
      const xRange = Math.max(0, rect.width / 2 - thumbSize / 2);
      const yRange = Math.max(0, rect.height / 2 - thumbSize / 2);
      const xOffset = clamp(side.servoLateral / LATERAL_LIMIT, -1, 1) * xRange;
      const yOffset = clamp(-side.servoUpdown / UPDOWN_LIMIT, -1, 1) * yRange;
      side.thumb.style.transform = `translate(${xOffset}px, ${yOffset}px)`;
    }

    function setJoystickValues(side, updown, lateral, shouldSend) {
      side.servoUpdown = clamp(parseInt(updown, 10) || 0, -UPDOWN_LIMIT, UPDOWN_LIMIT);
      side.servoLateral = clamp(parseInt(lateral, 10) || 0, -LATERAL_LIMIT, LATERAL_LIMIT);
      syncSideReadout(side);
      renderJoystickThumb(side);

      if (shouldSend) {
        controlStateDirty = true;
        if (statusSynced) {
          sendControlState();
        }
      }
    }

    function updateJoystickFromPointer(side, clientX, clientY, shouldSend) {
      const rect = side.pad.getBoundingClientRect();
      const xHalf = Math.max(1, rect.width / 2);
      const yHalf = Math.max(1, rect.height / 2);
      const xNorm = clamp((clientX - (rect.left + xHalf)) / xHalf, -1, 1);
      const yNorm = clamp((clientY - (rect.top + yHalf)) / yHalf, -1, 1);

      setJoystickValues(side, Math.round(-yNorm * UPDOWN_LIMIT), Math.round(xNorm * LATERAL_LIMIT), shouldSend);
    }

    function buildControlState() {
      return {
        inputMode: 'manual',
        leftServoUpdown: sides.left.servoUpdown,
        leftServoLateral: sides.left.servoLateral,
        leftMotorPwm: sides.left.motorPwm,
        leftMotorDir: sides.left.motorDir,
        rightServoUpdown: sides.right.servoUpdown,
        rightServoLateral: sides.right.servoLateral,
        rightMotorPwm: sides.right.motorPwm,
        rightMotorDir: sides.right.motorDir,
        leftLight: sides.left.lightOn,
        rightLight: sides.right.lightOn,
        leftLightBreaker: sides.left.lightBreakerActive,
        rightLightBreaker: sides.right.lightBreakerActive,
        leftFog: sides.left.fogOn,
        rightFog: sides.right.fogOn,
        motorLink: syncOptions.motorLink,
        motorMirror: syncOptions.motorMirror,
        eyeballLink: syncOptions.eyeballLink,
        eyeballMirror: syncOptions.eyeballMirror,
        lightLink: syncOptions.lightLink,
        fogLink: syncOptions.fogLink
      };
    }

    function updateSyncUi() {
      syncUi.motorLink.checked = !!syncOptions.motorLink;
      syncUi.motorMirror.checked = !!syncOptions.motorMirror;
      syncUi.eyeballLink.checked = !!syncOptions.eyeballLink;
      syncUi.eyeballMirror.checked = !!syncOptions.eyeballMirror;
      syncUi.lightLink.checked = !!syncOptions.lightLink;
      syncUi.fogLink.checked = !!syncOptions.fogLink;
      syncUi.motorMirror.disabled = !syncOptions.motorLink;
      syncUi.eyeballMirror.disabled = !syncOptions.eyeballLink;
    }

    function enforceSyncDependencies(changedKey) {
      if (changedKey === 'motorLink' && !syncOptions.motorLink) {
        syncOptions.motorMirror = false;
      }
      if (changedKey === 'eyeballLink' && !syncOptions.eyeballLink) {
        syncOptions.eyeballMirror = false;
      }
      if (!syncOptions.motorLink) {
        syncOptions.motorMirror = false;
      }
      if (!syncOptions.eyeballLink) {
        syncOptions.eyeballMirror = false;
      }
    }

    function sendControlState() {
      if (ws.readyState !== WebSocket.OPEN) {
        return;
      }

      ws.send(JSON.stringify(buildControlState()));
      controlStateDirty = false;
    }

    function setNodeUi(nodeId, connected, state, message, label) {
      const node = nodeUi.find(function(entry) {
        return entry.id === nodeId;
      });

      if (!node) {
        return;
      }

      node.light.className = 'signal-light signal-' + state;
      if (typeof label === 'string' && label.length > 0) {
        node.header.textContent = label;
      }
      node.connection.textContent = connected ? 'Connected' : 'Disconnected';
      node.connection.className = connected ? 'status-ok' : 'status-error';
      node.message.textContent = message;
    }

    function requestNodeTest(nodeId) {
      if (ws.readyState !== WebSocket.OPEN) {
        setNodeUi(nodeId, false, 'error', 'WebSocket disconnected');
        return;
      }

      setNodeUi(nodeId, true, 'sending', 'Sending signal...');
      ws.send(JSON.stringify({cmd: 'nodeTest', nodeId: nodeId}));
    }

    ws.onopen = function() {
      wsStatus.textContent = 'Connected';
      wsStatus.className = 'status-ok';
      ws.send(JSON.stringify({cmd: 'getStatus'}));
    };

    ws.onerror = function() {
      wsStatus.textContent = 'Error';
      wsStatus.className = 'status-error';
    };

    ws.onmessage = function(event) {
      const data = JSON.parse(event.data);
      const nodeCount = typeof data.nodeCount === 'number' ? data.nodeCount : 4;
      const connectedNodes = typeof data.connectedNodes === 'number'
        ? data.connectedNodes
        : (Array.isArray(data.nodes) ? data.nodes.filter(function(node) { return !!node.connected; }).length : 0);
      nodeStatus.textContent = connectedNodes + '/' + nodeCount + ' connected';
      nodeStatus.className = connectedNodes > 0 ? 'status-ok' : 'status-error';

      if (typeof data.servoUpdown === 'number' || typeof data.servoLateral === 'number') {
        setJoystickValues(sides.left,
          typeof data.servoUpdown === 'number' ? data.servoUpdown : sides.left.servoUpdown,
          typeof data.servoLateral === 'number' ? data.servoLateral : sides.left.servoLateral,
          false);
      }
      if (typeof data.leftServoUpdown === 'number' || typeof data.leftServoLateral === 'number') {
        setJoystickValues(sides.left,
          typeof data.leftServoUpdown === 'number' ? data.leftServoUpdown : sides.left.servoUpdown,
          typeof data.leftServoLateral === 'number' ? data.leftServoLateral : sides.left.servoLateral,
          false);
      }
      if (typeof data.rightServoUpdown === 'number' || typeof data.rightServoLateral === 'number') {
        setJoystickValues(sides.right,
          typeof data.rightServoUpdown === 'number' ? data.rightServoUpdown : sides.right.servoUpdown,
          typeof data.rightServoLateral === 'number' ? data.rightServoLateral : sides.right.servoLateral,
          false);
      }

      if (typeof data.motorPwm === 'number') {
        setHeadRotationAxis(sides.left, motorCommandToAxis(data.motorPwm, sides.left.motorDir));
      }
      if (typeof data.leftMotorPwm === 'number') {
        setHeadRotationAxis(sides.left, motorCommandToAxis(data.leftMotorPwm, sides.left.motorDir));
      }
      if (typeof data.rightMotorPwm === 'number') {
        setHeadRotationAxis(sides.right, motorCommandToAxis(data.rightMotorPwm, sides.right.motorDir));
      }

      if (typeof data.motorDir === 'number') {
        sides.left.motorDir = data.motorDir;
        setHeadRotationAxis(sides.left, motorCommandToAxis(sides.left.motorPwm, data.motorDir));
      }
      if (typeof data.leftMotorDir === 'number') {
        sides.left.motorDir = data.leftMotorDir;
        setHeadRotationAxis(sides.left, motorCommandToAxis(sides.left.motorPwm, data.leftMotorDir));
      }
      if (typeof data.rightMotorDir === 'number') {
        sides.right.motorDir = data.rightMotorDir;
        setHeadRotationAxis(sides.right, motorCommandToAxis(sides.right.motorPwm, data.rightMotorDir));
      }
      if (typeof data.leftLight === 'boolean') {
        sides.left.lightOn = data.leftLight;
        syncSideReadout(sides.left);
      }
      if (typeof data.rightLight === 'boolean') {
        sides.right.lightOn = data.rightLight;
        syncSideReadout(sides.right);
      }
      if (typeof data.leftLightBreaker === 'boolean') {
        sides.left.lightBreakerActive = data.leftLightBreaker;
        syncSideReadout(sides.left);
      }
      if (typeof data.rightLightBreaker === 'boolean') {
        sides.right.lightBreakerActive = data.rightLightBreaker;
        syncSideReadout(sides.right);
      }
      if (typeof data.leftFog === 'boolean') {
        sides.left.fogOn = data.leftFog;
        syncSideReadout(sides.left);
      }
      if (typeof data.rightFog === 'boolean') {
        sides.right.fogOn = data.rightFog;
        syncSideReadout(sides.right);
      }
      if (typeof data.leftBaseAngleDeg10 === 'number') {
        sides.left.baseAngleDeg10 = data.leftBaseAngleDeg10;
        renderBaseAngle(sides.left);
      }
      if (typeof data.rightBaseAngleDeg10 === 'number') {
        sides.right.baseAngleDeg10 = data.rightBaseAngleDeg10;
        renderBaseAngle(sides.right);
      }

      if (typeof data.motorLink === 'boolean') {
        syncOptions.motorLink = data.motorLink;
      }
      if (typeof data.motorMirror === 'boolean') {
        syncOptions.motorMirror = data.motorMirror;
      }
      if (typeof data.eyeballLink === 'boolean') {
        syncOptions.eyeballLink = data.eyeballLink;
      }
      if (typeof data.eyeballMirror === 'boolean') {
        syncOptions.eyeballMirror = data.eyeballMirror;
      }
      if (typeof data.lightLink === 'boolean') {
        syncOptions.lightLink = data.lightLink;
      }
      if (typeof data.fogLink === 'boolean') {
        syncOptions.fogLink = data.fogLink;
      }
      enforceSyncDependencies();
      updateSyncUi();

      if (Array.isArray(data.nodes)) {
        data.nodes.forEach(function(node) {
          if (!node || typeof node.id !== 'number') {
            return;
          }
          setNodeUi(
            node.id,
            !!node.connected,
            typeof node.testState === 'string' ? node.testState : 'idle',
            typeof node.testMessage === 'string' ? node.testMessage : 'Ready',
            typeof node.label === 'string' ? node.label : ''
          );
        });
      }

      statusSynced = true;
      if (controlStateDirty) {
        sendControlState();
      }
    };

    ['left', 'right'].forEach(function(key) {
      const side = sides[key];
      side.motorSlider.addEventListener('input', function() {
        setHeadRotationAxis(side, side.motorSlider.value);
        controlStateDirty = true;
        if (statusSynced) {
          sendControlState();
        }
      });

      side.stopButton.addEventListener('click', function() {
        setHeadRotationAxis(side, 0);
        controlStateDirty = true;
        if (statusSynced) {
          sendControlState();
        }
      });

      side.lightSwitch.addEventListener('change', function() {
        side.lightOn = !!side.lightSwitch.checked;
        controlStateDirty = true;
        if (statusSynced) {
          sendControlState();
        }
      });

      function bindMomentaryButton(button, onStateChanged) {
        let pressed = false;

        function setPressed(nextPressed) {
          if (pressed === nextPressed) {
            return;
          }
          pressed = nextPressed;
          onStateChanged(pressed);
          controlStateDirty = true;
          if (statusSynced) {
            sendControlState();
          }
        }

        button.addEventListener('pointerdown', function(event) {
          button.setPointerCapture(event.pointerId);
          setPressed(true);
          event.preventDefault();
        });

        function releaseFromPointer(event) {
          if (button.hasPointerCapture(event.pointerId)) {
            button.releasePointerCapture(event.pointerId);
          }
          setPressed(false);
        }

        button.addEventListener('pointerup', releaseFromPointer);
        button.addEventListener('pointercancel', releaseFromPointer);
        button.addEventListener('pointerleave', function() {
          setPressed(false);
        });
      }

      bindMomentaryButton(side.lightBreakerButton, function(isPressed) {
        side.lightBreakerActive = isPressed;
        syncSideReadout(side);
      });

      bindMomentaryButton(side.fogButton, function(isPressed) {
        side.fogOn = isPressed;
        syncSideReadout(side);
      });

      side.pad.addEventListener('pointerdown', function(event) {
        side.dragging = true;
        side.pad.setPointerCapture(event.pointerId);
        updateJoystickFromPointer(side, event.clientX, event.clientY, true);
        event.preventDefault();
      });

      side.pad.addEventListener('pointermove', function(event) {
        if (!side.dragging) {
          return;
        }
        updateJoystickFromPointer(side, event.clientX, event.clientY, true);
        event.preventDefault();
      });

      function finishDrag(event) {
        if (!side.dragging) {
          return;
        }
        side.dragging = false;
        if (side.pad.hasPointerCapture(event.pointerId)) {
          side.pad.releasePointerCapture(event.pointerId);
        }
      }

      side.pad.addEventListener('pointerup', finishDrag);
      side.pad.addEventListener('pointercancel', finishDrag);
    });

    nodeUi.forEach(function(node) {
      if (!node || !node.button) {
        return;
      }
      node.button.addEventListener('click', function() {
        requestNodeTest(node.id);
      });
    });

    Object.keys(syncUi).forEach(function(key) {
      syncUi[key].addEventListener('change', function() {
        syncOptions[key] = !!syncUi[key].checked;
        enforceSyncDependencies(key);
        updateSyncUi();
        controlStateDirty = true;
        if (statusSynced) {
          sendControlState();
        }
      });
    });

    window.addEventListener('load', function() {
      setJoystickValues(sides.left, 0, 0, false);
      setJoystickValues(sides.right, 0, 0, false);
      setHeadRotationAxis(sides.left, 0);
      setHeadRotationAxis(sides.right, 0);
      sides.left.lightOn = false;
      sides.right.lightOn = false;
      sides.left.lightBreakerActive = false;
      sides.right.lightBreakerActive = false;
      sides.left.fogOn = false;
      sides.right.fogOn = false;
      syncSideReadout(sides.left);
      syncSideReadout(sides.right);
      updateSyncUi();
      nodeUi.forEach(function(node) {
        setNodeUi(node.id, false, 'idle', 'Ready');
      });
    });

    window.addEventListener('resize', function() {
      renderJoystickThumb(sides.left);
      renderJoystickThumb(sides.right);
    });

    document.addEventListener('touchmove', function(event) {
      if (!event.target.closest('input[type="range"]')) {
        event.preventDefault();
      }
    }, { passive: false });

    // Disable browser context menus globally (desktop right-click and mobile long-press).
    document.addEventListener('contextmenu', function(event) {
      event.preventDefault();
    });
  </script>
</body>
</html>
)rawliteral";
}

}  // namespace th
