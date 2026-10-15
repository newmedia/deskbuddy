#!/usr/bin/env python3
"""Begeleide IMU-meting: vraagt per fase een kanteling en logt g/tilt naar sim-out/imu-measure.log.

Gebruik:  sg dialout -c ".venv/bin/python tools/imu-measure.py"
"""
import glob
import os
import re
import sys
import time

import serial

PHASES = [
    ("Rechtop laten staan", 5),
    ("Kantel naar LINKS (bovenkant naar links)", 5),
    ("Terug rechtop", 3),
    ("Kantel naar RECHTS (bovenkant naar rechts)", 5),
    ("Terug rechtop", 3),
    ("Kantel NAAR JE TOE (bovenkant naar jou)", 5),
    ("Terug rechtop", 3),
    ("Kantel VAN JE AF (bovenkant weg van jou)", 5),
]

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ports = glob.glob("/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_*-if00")
if not ports:
    sys.exit("Geen deskbuddy gevonden op USB.")
os.makedirs(os.path.join(root, "sim-out"), exist_ok=True)
log_path = os.path.join(root, "sim-out", "imu-measure.log")
pat = re.compile(r"g=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+) tilt=(-?[\d.]+),(-?[\d.]+)")

def connect():
    while True:
        try:
            p = glob.glob("/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_*-if00")
            if p:
                return serial.Serial(p[0], 115200, timeout=0.2)
        except serial.SerialException:
            pass
        time.sleep(0.3)


input("Pak de deskbuddy vast en druk op Enter om te starten... ")
s = connect()
with open(log_path, "w") as log:
    for phase, secs in PHASES:
        print(f"\n>>> {phase}  ({secs} s)")
        log.write(f"# {phase}\n")
        end = time.time() + secs
        while time.time() < end:
            try:
                s.write(b"status\n")
                deadline = time.time() + 0.25
                while time.time() < deadline:
                    line = s.readline().decode("utf-8", "replace").strip()
                    if not line:
                        continue
                    m = pat.search(line)
                    if m:
                        gx, gy, gz, tx, ty = m.groups()
                        left = end - time.time()
                        print(f"\r  {left:4.1f}s  g=({gx:>5},{gy:>5},{gz:>5})  tilt=({tx:>5},{ty:>5})   ", end="", flush=True)
                        log.write(f"{gx} {gy} {gz} {tx} {ty}\n")
                    else:
                        print(f"\n  [apparaat] {line}")
                        log.write(f"! {line}\n")
            except (serial.SerialException, OSError) as e:
                print(f"\n  !! USB-verbinding weg ({e.__class__.__name__}), opnieuw verbinden...")
                log.write(f"! USB-verbinding weg: {e}\n")
                try:
                    s.close()
                except Exception:
                    pass
                s = connect()
                print("  verbonden")
print(f"\n\nKlaar. Log: {log_path}")
