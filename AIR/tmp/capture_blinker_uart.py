"""Read-only ST-Link VCP capture; does not send bytes to PA3 or the module."""
from pathlib import Path
import json
import time
import serial

root = Path(__file__).resolve().parent.parent
report = {"port": "COM3", "baud": 9600, "read_only": True,
          "duration_seconds": 8, "json_packets": [], "parse_errors": []}
received = bytearray()
try:
    uart = serial.Serial(port=None, baudrate=9600, timeout=0.2)
    uart.dtr = False
    uart.rts = False
    uart.port = "COM3"
    with uart:
        uart.reset_input_buffer()
        deadline = time.monotonic() + report["duration_seconds"]
        while time.monotonic() < deadline:
            received.extend(uart.read(uart.in_waiting or 1))
except serial.SerialException as error:
    report["serial_error"] = str(error)

(root / "logs/hc06-blinker-serial.bin").write_bytes(received)
(root / "logs/hc06-blinker-serial.log").write_text(
    received.decode("utf-8", errors="replace"), encoding="utf-8")
lines = received.split(b"\n")
for index, line in enumerate(lines[:-1]):
    if not line:
        continue
    if index == 0 and not line.startswith(b"{"):
        report["discarded_initial_partial_line"] = line.decode("ascii", errors="replace")
        continue
    try:
        packet = json.loads(line)
        for key in ("temp", "humi"):
            if key in packet:
                assert isinstance(packet[key]["val"], (float, int))
        if "temp" in packet:
            assert packet["temp"]["uni"] == "\u2103"
        if "humi" in packet:
            assert packet["humi"]["uni"] == "%"
        report["json_packets"].append(packet)
    except (ValueError, KeyError, TypeError, AssertionError) as error:
        report["parse_errors"].append({"line": line.decode("ascii", errors="replace"),
                                      "error": str(error)})
report["bytes_received"] = len(received)
report["valid_json_packet_count"] = len(report["json_packets"])
report["phone_ui_verified"] = False
(root / "logs/hc06-blinker-serial.json").write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(json.dumps({k: v for k, v in report.items() if k != "json_packets"}, ensure_ascii=True))
if report["json_packets"]:
    print("Last UART packet: " + json.dumps(report["json_packets"][-1], ensure_ascii=True))
