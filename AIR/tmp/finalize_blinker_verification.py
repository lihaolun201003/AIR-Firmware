from pathlib import Path
import hashlib
import json
import re

root = Path(__file__).resolve().parent.parent


def sha(path):
    return hashlib.sha256((root / path).read_bytes()).hexdigest()


def log(path):
    raw = (root / path).read_bytes()
    return raw.decode("utf-16" if raw.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8")


build = log("logs/hc06-blinker-build.log")
tests = log("logs/hc06-blinker-native-tests.log")
upload = log("logs/hc06-blinker-upload.log")
assert "50 test cases: 50 succeeded" in tests
assert "Verified OK" in upload and "Resetting Target" in upload
assert "[SUCCESS]" in upload
resources = {}
for env in ("nucleo_f411re", "nucleo_f411re_env", "nucleo_f411re_debug"):
    section = build.split(f"Processing {env} (")[1].split("Processing ")[0]
    assert "[SUCCESS]" in section
    resources[env] = {
        "build": "SUCCESS",
        "static_ram_bytes": int(re.search(r"RAM:.*used (\d+) bytes", section)[1]),
        "flash_used_bytes": int(re.search(r"Flash:.*used (\d+) bytes", section)[1]),
    }

old = json.loads((root / "logs/hc06-env-verification.json").read_text(encoding="utf-8"))
unchanged = ["src/main.cpp", "include/pms5003st.h", "src/boot_animation.cpp",
             "src/boot_display.cpp", "include/boot_character_data.h",
             "test/test_pms_parser/test_main.cpp"]
for path in unchanged:
    assert sha(path) == old["files"][path]["sha256"], f"Unexpected change: {path}"
assert sha("firmware/air-hc06-blinker-20261010.bin") == sha(".pio/build/nucleo_f411re/firmware.bin")
assert sha("firmware/air-hc06-blinker-20261010.elf") == sha(".pio/build/nucleo_f411re/firmware.elf")
capture = json.loads((root / "logs/hc06-blinker-serial.json").read_text(encoding="utf-8"))
assert not capture["parse_errors"] and capture["valid_json_packet_count"] >= 1
files = ["include/air_config.h", "include/air_bluetooth.h", "src/air_bluetooth.cpp",
         "platformio.ini", "test/test_air_bluetooth/test_main.cpp", "README.md",
         "docs/Blinker-integration.md", "docs/HC06-integration.md",
         "firmware/air-hc06-blinker-20261010.bin", "firmware/air-hc06-blinker-20261010.elf"]
report = {
    "client_date": "2026-10-10", "board": "NUCLEO-F411RE",
    "task": "Adapt STM32 UART telemetry to Blinker temp/humi number widgets",
    "default_environment": "nucleo_f411re", "legacy_environment": "nucleo_f411re_env",
    "protocol": {"default": "Blinker JSON lines", "encoding": "ASCII with JSON Unicode escape",
                 "terminator": "LF (0x0A)", "baud": 9600, "uart": "USART2 RX PA3 / TX PA2",
                 "widgets": ["pm1", "pm25", "pm10", "hcho", "temp", "humi"],
                 "number_properties": ["val", "uni"], "temperature_unit": "\u2103",
                 "humidity_unit": "%", "report_interval_ms": 1000,
                 "status_query": {"get": "state"}, "status_reply": {"state": "connected"},
                 "legacy_switch": "AIR_BLUETOOTH_BLINKER=0",
                 "tested_input_example": "$ENV,10,20,30,0.015,28.6,72.5#",
                 "tested_temp_value": 28.6, "tested_humi_value": 72.5,
                 "example_is_hardcoded_in_firmware": False},
    "resources": resources,
    "tests": {"native": {"total_passed": 25, "bluetooth": 19, "pms": 6},
              "native_env": {"total_passed": 25, "bluetooth": 19, "pms": 6},
              "legacy_blinker_only_service_cases": "two mode-specific cases not applicable in legacy mode",
              "failures": 0, "log": "logs/hc06-blinker-native-tests.log"},
    "preserved_source_hashes_match_previous_firmware": unchanged,
    "hardware": {"upload": "Verified OK", "reset": True,
                 "upload_log": "logs/hc06-blinker-upload.log",
                 "read_only_uart_capture": "logs/hc06-blinker-serial.json",
                 "valid_json_packet_count": capture["valid_json_packet_count"],
                 "last_uart_packet": capture["json_packets"][-1],
                 "observation": "STM32 USART2 TX remains observable through ST-Link VCP COM3; bridge states not physically measured",
                 "phone": "iPhone; user reported successful device connection",
                 "phone_ui_verified_by_agent": False,
                 "lcd_visual_test": "not performed; source preserved"},
    "backup": "backups/before-blinker-20261010",
    "files": {path: {"bytes": (root / path).stat().st_size, "sha256": sha(path)} for path in files},
    "sources": ["https://github.com/blinker-iot/blinker-library/blob/master/src/Functions/BlinkerNumber.h",
                "https://github.com/blinker-iot/blinker-library/blob/master/src/Adapters/BlinkerBLE.h",
                "https://github.com/blinker-iot/blinker-library/blob/master/src/Blinker/BlinkerApi.h"],
}
(root / "logs/hc06-blinker-verification.json").write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"verified": True, "resources": resources,
                  "unchanged_source_files": len(unchanged),
                  "serial_json_packets": capture["valid_json_packet_count"],
                  "report": "logs/hc06-blinker-verification.json"}, ensure_ascii=True))
