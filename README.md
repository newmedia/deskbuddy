<p align="center"><img src="firmware/web/icon.svg" alt="deskbuddy.dev" width="128" height="128"></p>

# deskbuddy.dev

<p align="center">
  <a href="https://deskbuddy.dev"><b>deskbuddy.dev</b></a> ·
  <a href="https://newmedia.github.io/deskbuddy/flash/"><b>Install firmware</b></a> ·
  <a href="https://newmedia.github.io/deskbuddy/">Web app (PWA)</a> ·
  <a href="https://github.com/newmedia/deskbuddy/releases">All versions</a>
</p>

A desk robot with a round AMOLED screen, built on a
[Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm)
in a 3D-printed case. The board still runs the original closed-source firmware;
the custom deskbuddy firmware lives in `firmware/`.

## Installing
Without building anything: open **https://newmedia.github.io/deskbuddy/flash/** in Chrome or Edge,
connect the board over USB and click *Connect and install*. Note: this replaces the firmware on the
device (make a backup first if you want to keep the original).

## Documentation
- [docs/hardware.md](docs/hardware.md): the board, the chips, the pin mapping and how it shows up on USB
- [docs/original-firmware.md](docs/original-firmware.md): what the original firmware does and what is still open
- [docs/firmware.md](docs/firmware.md): the custom deskbuddy firmware: controls, structure, building and flashing
- [docs/regression-test.md](docs/regression-test.md): using and extending the automated regression test on the web emulator
- [docs/multi-buddy.md](docs/multi-buddy.md): idea and design for multiple buddies that react to each other
- [docs/plan.md](docs/plan.md): plan for custom deskbuddy firmware, web/PWA, desktop app and HA

## Quick start
```bash
sudo usermod -aG dialout $USER   # once, then log in again
tools/setup.sh                   # venv with esptool + pyserial
tools/chip-info.sh               # chip/flash info (read-only)
tools/backup-flash.sh            # full flash backup to backups/
tools/monitor.sh                 # view the serial log
tools/sim.sh                     # firmware preview on the PC (sim-out/demo.gif)
tools/webemu.sh                  # web emulator: http://localhost:8765/emu
tools/webemu-test.sh             # regression test without a device (exit code 0 = pass)
tools/flash.sh                   # flash the deskbuddy firmware (asks for confirmation)
```

The device shows up as `303a:1001` (Espressif USB JTAG/serial) on `/dev/ttyACM0`.
Set `PORT=...` to use a different port.

> Always make a backup before you flash. The original firmware is not open source.
