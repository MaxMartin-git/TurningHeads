# TurningHeads

TurningHeads is a WiFi-coordinated ESP32 project for controlling a two-axis servo rig and a DC motor. The coordinator runs a web UI and WebSocket server, while one or more nodes connect over TCP to receive motor commands.

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
- HTTP web interface with joystick-style servo control
- WebSocket updates for servo position, motor PWM, motor direction, and node connection status
- TCP server on port `5000` for node motor commands
- SCServo control for up/down and lateral servos via UART
- PWM output for DC motor control on the node

## Hardware

- ESP32-C3 development board
- SCServo-compatible servo controller or direct UART servo interface
- DC motor driver with PWM input
- Node device connects to the coordinator’s WiFi AP

## Build & Upload

1. Install PlatformIO in VS Code.
2. Open this project folder in VS Code.
3. Select the desired environment in `platformio.ini`:
   - `coordinator` - coordinator firmware with web UI and servo control
   - `node` - node firmware for motor control
   - `servo_id_setter` - servo ID configuration helper
   - `servo_id_reader` - servo ID reader helper
   - `servo_center_setter` - servo center calibration helper
   - `main_test` - basic board alive test
4. Build and upload using PlatformIO commands or the status bar buttons.

## Coordinator Usage

1. Flash `src/main.cpp` with the `coordinator` environment.
2. Power the board and connect to the WiFi AP `ESP_TH` with password `TurningHeads123`.
3. Open a browser and go to `http://192.168.4.1/`.
4. Use the on-screen joystick and motor controls to move the servos and send motor commands to the node.

## Node Usage

1. Flash `src/main_node.cpp` with the `node` environment.
2. Configure the node wiring so `GPIO3` provides PWM to the motor driver.
3. Connect the node to the coordinator WiFi AP.
4. The node will attempt to connect to `192.168.4.1:5000` and receive motor commands from the coordinator.

## Notes

- The coordinator uses UART pins `GPIO21` (TX) and `GPIO20` (RX) for servo control.
- The node assumes the coordinator access point SSID and password match exactly.
- Adjust servo limits in `src/main.cpp` if your mechanics require different travel ranges.

## License

Adapt or reuse this code as needed for educational and prototyping purposes.
