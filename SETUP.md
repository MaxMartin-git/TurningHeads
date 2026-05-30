# TurningHeads Wireless Control - Prototyp Setup

## Übersicht
- **Coordinator ESP32-C3**: Access Point + WebSocket + TCP-Server
- **Node ESP32-C3**: TCP-Client, empfängt Motor-Befehle
- **Smartphone**: WebSocket-Client mit Web-UI (Slider für Motor-PWM 0..255)

## Komponenten & Pins

### Coordinator (dein aktueller TurningHeads Projekt)
- GPIO 3: PWM Motor 1 (später: DC-Motor)
- UART1 (GPIO 21 TX, GPIO 20 RX): SC09 Servo (38400 bps, später integrierbar)
- WiFi AP: SSID=`TurningHeads`, Pass=`12345`

### Node (zweiter ESP32-C3)
- GPIO 3: PWM Motor 1 (empfängt Werte vom Coordinator)
- Verbindet sich zum AP des Coordinators

## Bauen & Flashen

### 1. **Coordinator Flashen** (aktueller Ordner)
```bash
platformio run -e coordinator --target upload
```

### 2. **Node Flashen** (neuer Ordner)
```bash
platformio run -e node --target upload
```

### 3. **Servo-Mitte speichern**
```bash
platformio run -e servo_center_setter --target upload
```

Nach dem Flashen den Servo von Hand in die gewünschte Mittelposition bringen, dann einschalten. Das Skript speichert die aktuelle Stellung automatisch beim Start und fährt den Servo dabei nicht an.

Oder in VS Code oben rechts die Umgebung (`[coordinator]` oder `[node]`) wechseln und `Upload` klicken.

## So funktioniert es

### Phase 1: Start
1. Flashe **Coordinator** auf ESP32-C3 #1
2. Flashe **Node** auf ESP32-C3 #2
3. Coordinator startet automatisch WLAN-AP `TurningHeads`
4. Node verbindet sich automatisch zum AP

### Phase 2: Steuerung
1. Öffne mit dem **Smartphone** das WLAN `TurningHeads`, Passwort: `12345`
2. Starte einen Browser und navigiere zu `http://192.168.4.1`
3. Du siehst die Web-UI mit:
   - Slider für Motor-Speed (0..255)
   - Status: WebSocket + Node-Verbindung

### Phase 3: Live-Steuerung
- Bewege den Slider → Wert wird via WebSocket zum Coordinator gesendet
- Coordinator setzt lokal den PWM und sendet über TCP an Node
- Node empfängt und setzt den PWM an seinem GPIO 3
- Beide Motor sollten sich zur gleichen Zeit bewegen (gute Synchronität)

## Datenfluss

```
Smartphone WebSocket       Coordinator         TCP         Node
    |             "M:200"       |          "M:200\n"        |
    |---[JSON]-------> :80 -------> :5000 -------> GPIO 3
    |             (PWM lokal)                  (PWM setzen)
    |<--- [Status JSON] ---------|
```

## Nächste Schritte (später Erweiterungen)

1. **SC09 Servo integrieren**
   - UART1 auf Coordinator starten (38400 bps, wie alter Code)
   - Servo über SCServo-Lib ansteuern
   - Zusätzlicher Slider im Web-UI für Servo-Position

2. **PWM-Joystick statt Slider**
   - HTML Canvas Joystick für beide Werte (X/Y)
   - Mapping z.B.: X → Motor1 Speed, Y → Motor2 Speed

3. **Mehrere Node-ESPs**
   - Coordinator an mehrere Nodes Befehle streamen
   - JSON-Format erweitern: `{"motor1": 100, "motor2": 150}`

4. **Servo auf Node**
   - Node mit UART für Servo ausrüsten
   - Nachricht-Format erweitern: `{"motor": 100, "servo": 512}`

5. **Persistierung**
   - ~~AP-Passwort änderbar über Web-UI~~
   - ~~WLAN-Einstellungen speichern~~
   - (Für Prototyp erstmal nicht wichtig)

## Troubleshooting

### Node verbindet sich nicht zum Coordinator
1. Prüfe SSID und Passwort in `src_node/main.cpp` 
2. Prüfe IP des Coordinators (sollte 192.168.4.1 sein)
3. Serielle Ausgabe der Node anschauen: `platformio device monitor -e node`

### Smartphone kann sich nicht zu `TurningHeads` verbinden
1. Serielle Monitor Coordinator: `platformio device monitor -e coordinator`
2. Suche nach `[WiFi] AP started`
3. Wenn nicht, prüfe dass Coordinator hochgefahren ist

### Motor reagiert nicht auf Slider
1. Prüfe PWM-Pin (sollte GPIO 3 sein, ist konfigurierbar)
2. Prüfe dass Motor an Vin/GND angeschlossen ist
3. Serielle Ausgabe: schau ob `[MOTOR]` Zeile gedruckt wird

### WebSocket-Status zeigt "Disconnected"
1. Das ist normal in den ersten Sekunden nach Neustart
2. Warte 2-3 Sekunden
3. Aktualisiere die Web-Seite (F5)

## Code-Struktur Erklärung

### Coordinator main.cpp
- **WiFi AP Setup**: `WiFi.softAP()` am Anfang
- **Web-Server**: `AsyncWebServer` hostet HTML-Seite unter `/`
- **WebSocket Handler**: `onWebSocketEvent()` parst Motor-Wert und leitet weiter
- **TCP-Server**: `tcpServer.begin()` auf Port 5000
- **Loop**: TCP Heartbeat all 500ms und Verbindungsverwaltung

### Node main.cpp
- **WiFi STA Connect**: verbindet sich zum Coordinator-AP
- **TCP-Client**: stellt aktive Verbindung zu Coordinator her
- **Parser**: sucht nach `M:` Prefix in empfangenen Zeilen
- **Loop**: reconnect wenn verbindung abbricht

## Hardware-Anforderungen
- 2 x ESP32-C3-Zero (oder andere ESP32-C3)
- 2 x DC-Motor mit PWM-Regler (oder direkt GPIO-PWM)
- 1 x SC09 Servo (optional, später)
- Stromversorgung für Motors (separate Batterie/Netzteil)
- USB-Kabel zum Programmieren

## Sicherheit (wichtig für Prototyp!)
- ⚠️ WLAN ohne Verschlüsselung (offenes Netzwerk)
- ⚠️ Jeder in Reichweite kann steuern
- Für Produktion: WPA2, API-Key hinzufügen
- Für Prototyp: OK wenn sicherer Bereich (Labor, Wohnung)

---

Viel Erfolg! 🎮
