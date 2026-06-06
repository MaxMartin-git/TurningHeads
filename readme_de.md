# TurningHeads

TurningHeads ist ein WiFi-koordiniertes ESP32-Projekt zur Steuerung eines zweiachsigen Servo-Rigs und eines Gleichstrommotors. Der Koordinator betreibt eine Weboberfläche und einen WebSocket-Server, während ein oder mehrere Knoten über TCP verbinden, um Motorbefehle zu empfangen.

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
- HTTP-Weboberfläche mit joystickähnlicher Servo-Steuerung
- WebSocket-Updates für Servo-Position, Motor-PWM, Motordrehrichtung und Knotenverbindungsstatus
- TCP-Server auf Port `5000` für Knoten-Motorbefehle
- SCServo-Steuerung für Auf/Ab- und Quer-Servos über UART
- PWM-Ausgang zur Steuerung des DC-Motors am Knoten

## Hardware

- ESP32-C3-Entwicklungsboard
- SCServo-kompatibler Servo-Controller oder direkte UART-Servo-Schnittstelle
- Gleichstrommotor-Treiber mit PWM-Eingang
- Knotengerät verbindet sich mit dem WiFi-AP des Koordinators

## Erstellen & Hochladen

1. Installiere PlatformIO in VS Code.
2. Öffne diesen Projektordner in VS Code.
3. Wähle die gewünschte Umgebung in `platformio.ini`:
   - `coordinator` - Koordinator-Firmware mit Weboberfläche und Servo-Steuerung
   - `node` - Knoten-Firmware für die Motorsteuerung
   - `servo_id_setter` - Hilfsprogramm zum Konfigurieren der Servo-ID
   - `servo_id_reader` - Hilfsprogramm zum Lesen der Servo-ID
   - `servo_center_setter` - Hilfsprogramm zur Kalibrierung des Servo-Mittelpunkts
   - `main_test` - Basistest zum Überprüfen der Board-Lebensfähigkeit
4. Baue und lade mit PlatformIO-Befehlen oder den Schaltflächen in der Statusleiste.

## Verwendung des Koordinators

1. Flashe `src/main.cpp` mit der Umgebung `coordinator`.
2. Schließe das Board an die Stromversorgung an und verbinde dich mit dem WiFi-AP `ESP_TH` mit dem Passwort `TurningHeads123`.
3. Öffne einen Browser und rufe `http://192.168.4.1/` auf.
4. Verwende den Bildschirm-Joystick und die Motorsteuerung, um die Servos zu bewegen und Motorbefehle an den Knoten zu senden.

## Verwendung des Knotens

1. Flashe `src/main_node.cpp` mit der Umgebung `node`.
2. Konfiguriere die Verkabelung des Knotens so, dass `GPIO3` PWM an den Motor-Treiber liefert.
3. Verbinde den Knoten mit dem WiFi-AP des Koordinators.
4. Der Knoten versucht, sich mit `192.168.4.1:5000` zu verbinden und Motorbefehle vom Koordinator zu empfangen.

## Hinweise

- Der Koordinator verwendet die UART-Pins `GPIO21` (TX) und `GPIO20` (RX) für die Steuerung der Servos.
- Der Knoten setzt voraus, dass SSID und Passwort des Koordinator-Zugangspunkts genau übereinstimmen.
- Passe die Servo-Grenzen in `src/main.cpp` an, wenn deine Mechanik andere Bewegungsbereiche benötigt.

## Lizenz

Passe diesen Code nach Bedarf für Bildungs- und Prototyping-Zwecke an oder verwende ihn wieder.
