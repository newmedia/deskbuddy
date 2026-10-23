#!/usr/bin/env python3
"""Guided IMU measurement: asks for a tilt in each phase and logs g/tilt to sim-out/imu-measure.log.

Usage:  sg dialout -c ".venv/bin/python tools/imu-measure.py"
"""
import glob
import os
import re
import sys
import time

import serial

PHASES = [
    ("Leave upright", 5),
    ("Tilt LEFT (top towards the left)", 5),
    ("Back upright", 3),
    ("Tilt RIGHT (top towards the right)", 5),
    ("Back upright", 3),
    ("Tilt TOWARDS YOU (top towards you)", 5),
    ("Back upright", 3),
    ("Tilt AWAY FROM YOU (top away from you)", 5),
]

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ports = glob.glob("/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_*-if00")
if not ports:
    sys.exit("No deskbuddy found on USB.")
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


input("Pick up the deskbuddy and press Enter to start... ")
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
                        print(f"\n  [device] {line}")
                        log.write(f"! {line}\n")
            except (serial.SerialException, OSError) as e:
                print(f"\n  !! USB connection lost ({e.__class__.__name__}), reconnecting...")
                log.write(f"! USB connection lost: {e}\n")
                try:
                    s.close()
                except Exception:
                    pass
                s = connect()
                print("  connected")
print(f"\n\nDone. Log: {log_path}")
