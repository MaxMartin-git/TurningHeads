# TurningHeads

TurningHeads ist ein WiFi-koordiniertes ESP32-Projekt zur Steuerung gespiegelter Left/Right-Base- und Satellite-Nodes. Der Koordinator betreibt die Weboberfläche und den WebSocket-Server, fungiert gleichzeitig als lokale Base_L und verteilt seitenbezogene Befehle über TCP.

## Projektstruktur

- `platformio.ini` - PlatformIO-Umgebungen und Board-Konfiguration.
- `src/main.cpp` - Koordinator-Firmware für den ESP32-C3.
- `src/main_node.cpp` - Knoten-Firmware, die sich mit dem Koordinator verbindet und einen PWM-Motor antreibt.
- `src/servo_id_setter.cpp` - Hilfs-Build-Umgebung zum Setzen von Servo-IDs.
- `src/servo_id_reader.cpp` - Hilfs-Build-Umgebung zum Lesen von Servo-IDs.
- `src/servo_center_setter.cpp` - Hilfs-Build-Umgebung zum Einstellen der Servo-Mittelpunkt-Offsets.
- `src/main_test.cpp` - Minimale Test-Firmware zur Überprüfung, ob das Board lebt.

## Funktionen

- Der Koordinator erstellt einen WiFi-Zugangspunkt: `ESP_TH` / `TurningHeads123`
- Kompakte HTTP-Weboberfläche mit gedoppelten Left/Right-Bedienelementen
- Zwei Joystick-Pads (links/rechts) für seitengetrennte Servo-Ziele
- Zwei unabhängige Motor-Bedienelemente (PWM + Richtung) für linke/rechte Base
- WebSocket-Status für beide Seiten plus Knoten-Verbindung/Testzustand
- TCP-Server auf Ports `5001..5003` für node-spezifisches Routing
- SCServo-Steuerung der lokalen linken Servos über UART

## Hardware

- ESP32-C3-Entwicklungsboard
- SCServo-kompatibler Servo-Controller oder direkte UART-Servo-Schnittstelle
- Gleichstrommotor-Treiber mit PWM-Eingang
- Bis zu drei Remote-Knoten verbinden sich mit dem WiFi-AP des Koordinators

## Topologie

- Koordinator: Base_L (lokal)
- Node 1: Satellite_L
- Node 2: Base_R
- Node 3: Satellite_R

Gemeinsame Rollen- und Capabilities-Definitionen liegen unter `src/shared/`.

## Erstellen & Hochladen

1. Installiere PlatformIO in VS Code.
2. Öffne diesen Projektordner in VS Code.
3. Wähle die gewünschte Umgebung in `platformio.ini`:
   - `base_L_as_coordinator` - Base_L-Koordinator-Firmware mit Weboberfläche und Servo-Steuerung
   - `Satellite_L` - Satellite_L-Firmware (NODE_ID=1)
   - `Base_R` - Base_R-Firmware (NODE_ID=2)
   - `Satellite_R` - Satellite_R-Firmware (NODE_ID=3)
   - `servo_id_setter` - Hilfsprogramm zum Konfigurieren der Servo-ID
   - `servo_id_reader` - Hilfsprogramm zum Lesen der Servo-ID
   - `servo_center_setter` - Hilfsprogramm zur Kalibrierung des Servo-Mittelpunkts
   - `main_test` - Basistest zum Überprüfen der Board-Lebensfähigkeit
4. Baue und lade mit PlatformIO-Befehlen oder den Schaltflächen in der Statusleiste.

## Verwendung des Koordinators

1. Flashe `src/main.cpp` mit der Umgebung `base_L_as_coordinator`.
2. Schließe das Board an die Stromversorgung an und verbinde dich mit dem WiFi-AP `ESP_TH` mit dem Passwort `TurningHeads123`.
3. Öffne einen Browser und rufe `http://192.168.4.1/` auf.
4. Verwende die Left/Right-Bedienelemente auf der Oberfläche, um beide Seiten getrennt zu steuern.

## Verwendung des Knotens

1. Flashe `src/main_node.cpp` mit einer der Node-Umgebungen (`Satellite_L`, `Base_R`, `Satellite_R`).
2. Konfiguriere die Verkabelung des Knotens so, dass `GPIO3` PWM an den Motor-Treiber liefert.
3. Verbinde den Knoten mit dem WiFi-AP des Koordinators.
4. Der Knoten verbindet sich auf seinen dedizierten Port (`5001`, `5002` oder `5003`) und führt nur rollenspezifische Befehle aus:
   - Base-Nodes: Motorbefehle
   - Satellite-Nodes: Servo-Befehle

## Hinweise

- Der Koordinator verwendet die UART-Pins `GPIO21` (TX) und `GPIO20` (RX) für die Steuerung der Servos.
- Der Knoten setzt voraus, dass SSID und Passwort des Koordinator-Zugangspunkts genau übereinstimmen.
- Passe die Servo-Grenzen in `src/main.cpp` an, wenn deine Mechanik andere Bewegungsbereiche benötigt.

## Lizenz

Passe diesen Code nach Bedarf für Bildungs- und Prototyping-Zwecke an oder verwende ihn wieder.
