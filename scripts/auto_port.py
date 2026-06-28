from serial.tools import list_ports

Import("env")

TARGET_VID = 0x303A
TARGET_PID = 0x1001

ports = list(list_ports.comports())
matches = [p for p in ports if p.vid == TARGET_VID and p.pid == TARGET_PID]

if len(matches) == 1:
    port = matches[0].device
    env.Replace(UPLOAD_PORT=port)
    env.Replace(MONITOR_PORT=port)
    print("[auto_port] Using {}".format(port))
elif len(matches) == 0:
    print("[auto_port] No matching ESP32-C3 device found (VID:PID 303A:1001)")
else:
    found = ", ".join([p.device for p in matches])
    print("[auto_port] Multiple matching ESP32-C3 devices: {}".format(found))
    print("[auto_port] Please use --upload-port to select one")
