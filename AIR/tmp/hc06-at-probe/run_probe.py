"""Read HC-06 UART responses using STM32, then always restore the saved ELF."""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
import time

import serial

root = Path(__file__).resolve().parents[2]
pio_root = Path.home() / ".platformio"
openocd = pio_root / "packages/tool-openocd/bin/openocd.exe"
scripts = pio_root / "packages/tool-openocd/openocd/scripts"
nm = pio_root / "packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-nm.exe"
probe = root / "tmp/hc06-at-probe/.pio/build/nucleo_f411re/firmware.elf"
original = root / "firmware/air-hc06-env-20261010.elf"
capture_path = root / "logs/hc06-address-probe.bin"
report_path = root / "logs/hc06-address-probe.json"

assert original.is_file() and probe.is_file() and scripts.is_dir()
expected = json.loads((root / "logs/hc06-env-verification.json").read_text(encoding="utf-8"))
original_hash = hashlib.sha256(original.read_bytes()).hexdigest()
assert original_hash == expected["files"][original.relative_to(root).as_posix()]["sha256"]

symbols = subprocess.check_output([str(nm), "-S", "--defined-only", str(probe)], text=True)
symbol = re.search(r"^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+\w\s+hc06Probe$", symbols, re.M)
assert symbol, "Probe SRAM symbol not found"
address, size = (int(part, 16) for part in symbol.groups())
assert size == 2336 and 0x20000000 <= address < 0x20020000

base = [str(openocd), "-s", str(scripts), "-f", "interface/stlink.cfg",
        "-f", "target/stm32f4x.cfg", "-c", "gdb_port disabled",
        "-c", "tcl_port disabled", "-c", "telnet_port disabled"]
report = {"client_date": "2026-10-10", "uart": "USART2 PA3 RX / PA2 TX, 9600 8N1",
          "phone_disconnected": "confirmed by user", "queries_are_read_only": True,
          "sram_address": hex(address), "records": [], "addresses": [],
          "original_firmware": original.relative_to(root).as_posix(),
          "original_elf_sha256": original_hash, "original_firmware_restored": False}


def run_openocd(commands, log_name):
    argv = list(base)
    for command in commands:
        argv += ["-c", command]
    result = subprocess.run(argv, cwd=root, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=40)
    output = result.stdout.decode("utf-8", errors="replace")
    (root / "logs" / log_name).write_text(output, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"OpenOCD failed ({result.returncode}); see logs/{log_name}")
    return output


console = None
restore_required = False
failure = None
try:
    try:
        console = serial.Serial(port=None, baudrate=9600, timeout=0)
        console.dtr = False
        console.rts = False
        console.port = "COM3"
        console.open()
        console.reset_input_buffer()
    except serial.SerialException as error:
        report["stlink_vcp_read_error"] = str(error)
        if console:
            console.close()
        console = None

    restore_required = True
    output = run_openocd([f"program {{{probe.as_posix()}}} verify reset exit"],
                         "hc06-address-probe-upload.log")
    assert "Verified OK" in output
    print("Read-only UART probe uploaded and verified; collecting replies.", flush=True)
    time.sleep(12)
    if console:
        received = console.read(console.in_waiting)
        report["stlink_vcp_read_only_capture"] = received.decode("ascii", errors="replace")
        report["stlink_vcp_capture_hex"] = received.hex()

    run_openocd(["init", "halt",
                 f"dump_image {{{capture_path.as_posix()}}} {hex(address)} {size}",
                 "resume", "shutdown"], "hc06-address-probe-read.log")
    payload = capture_path.read_bytes()
    magic, done, count, uptime = struct.unpack_from("<IIII", payload)
    report.update({"magic": hex(magic), "probe_complete": bool(done), "uptime_ms": uptime})
    assert magic == 0x36435048 and done == 1 and count == 10
    pattern = re.compile(r"(?i)(?<![0-9a-f])(?:[0-9a-f]{12}|[0-9a-f]{2}(?::[0-9a-f]{2}){5}|[0-9a-f]{4}:[0-9a-f]{2}:[0-9a-f]{6})(?![0-9a-f])")
    for index in range(count):
        offset = 16 + index * 232
        command = payload[offset:offset + 32].split(b"\0", 1)[0].decode("ascii")
        length, overflow = struct.unpack_from("<II", payload, offset + 224)
        assert length <= 191
        raw = payload[offset + 32:offset + 32 + length]
        reply = raw.decode("ascii", errors="replace")
        report["records"].append({"command": command, "reply": reply,
                                  "reply_hex": raw.hex(), "length": length, "overflow": overflow})
        if "ADDR" in command:
            for match in pattern.finditer(reply):
                digits = match[0].replace(":", "").upper()
                formatted = ":".join(digits[i:i + 2] for i in range(0, 12, 2))
                if formatted not in report["addresses"]:
                    report["addresses"].append(formatted)
except Exception as error:
    failure = str(error)
    report["probe_error"] = failure
finally:
    if console:
        console.close()
    if restore_required:
        try:
            output = run_openocd([f"program {{{original.as_posix()}}} verify reset exit"],
                                 "hc06-address-probe-restore.log")
            assert "Verified OK" in output and "Resetting Target" in output
            report["original_firmware_restored"] = True
            report["restore_verification"] = "Verified OK"
            print("Original ENV firmware restored, verified, and reset.", flush=True)
        except Exception as error:
            report["restore_error"] = str(error)
            failure = str(error)
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

print(json.dumps(report, ensure_ascii=False, indent=2), flush=True)
if failure:
    raise SystemExit(1)
