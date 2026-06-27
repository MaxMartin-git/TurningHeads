English | [中文](readme_cn.md) | [Deutsch](readme_de.md)

# TurningHeads

TurningHeads is a WiFi-coordinated ESP32 project for controlling mirrored left/right base and satellite nodes. The coordinator runs a web UI and WebSocket server, acts as the local left base (Base_L), and routes side-specific commands to remote base/satellite nodes over TCP.

## Project Structure

- `platformio.ini` - PlatformIO environments and board configuration.
- `src/main.cpp` - Coordinator firmware for the ESP32-C3.
- `src/main_node.cpp` - Node firmware that connects to the coordinator and drives a PWM motor.
- `src/servo_id_setter.cpp` - Utility build environment for setting servo IDs.
- `src/servo_id_reader.cpp` - Utility build environment for reading servo IDs.
- `src/servo_center_setter.cpp` - Utility build environment for setting servo center offsets.
- `src/main_test.cpp` - Minimal test firmware used to verify the board is alive.

## Features

- Coordinator creates a WiFi access point: `ESP_TH` / `TurningHeads123`
- Compact HTTP web interface with duplicated Left/Right controls
- Two joystick pads (left/right) for side-specific servo targets
- Two independent motor controls (PWM + direction) for left and right base nodes
- WebSocket updates for dual-side state plus node connection/test status
- TCP server on ports `5001..5003` for per-node command routing
- SCServo control for local left-side servos via UART

## Hardware

- ESP32-C3 development board
- SCServo-compatible servo controller or direct UART servo interface
- DC motor driver with PWM input
- Up to three remote node devices connect to the coordinator’s WiFi AP

## Topology

- Coordinator: Base_L (local)
- Node 1: Satellite_L
- Node 2: Base_R
- Node 3: Satellite_R

All shared role/capability definitions live under `src/shared/`.

## Build & Upload

1. Install PlatformIO in VS Code.
2. Open this project folder in VS Code.
3. Select the desired environment in `platformio.ini`:
   - `coordinator` - coordinator firmware with web UI and servo control
   - `node1` - node firmware for motor control (NODE_ID=1)
   - `servo_id_setter` - servo ID configuration helper
   - `servo_id_reader` - servo ID reader helper
   - `servo_center_setter` - servo center calibration helper
   - `main_test` - basic board alive test
4. Build and upload using PlatformIO commands or the status bar buttons.

## Coordinator Usage

1. Flash `src/main.cpp` with the `coordinator` environment.
2. Power the board and connect to the WiFi AP `ESP_TH` with password `TurningHeads123`.
3. Open a browser and go to `http://192.168.4.1/`.
4. Use the on-screen Left/Right controls to drive each side independently.

## Node Usage

1. Flash `src/main_node.cpp` with one of the node environments (`node1`, `node2`, `node3`).
2. Configure the node wiring so `GPIO3` provides PWM to the motor driver.
3. Connect the node to the coordinator WiFi AP.
4. The node will attempt to connect to its dedicated port (`5001`, `5002`, or `5003`) and only execute role-appropriate commands:
   - Base nodes: motor commands
   - Satellite nodes: servo commands

## Notes

- The coordinator uses UART pins `GPIO21` (TX) and `GPIO20` (RX) for servo control.
- The node assumes the coordinator access point SSID and password match exactly.
- Adjust servo limits in `src/main.cpp` if your mechanics require different travel ranges.

## License

Adapt or reuse this code as needed for educational and prototyping purposes.
