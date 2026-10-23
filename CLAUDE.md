# CLAUDE.md

Project around a deskbuddy desk robot. Hardware: Waveshare ESP32-S3-Touch-AMOLED-1.75 (ESP32-S3R8,
16 MB flash, 466×466 AMOLED with CO5300 + CST9217 touch). Details and pins are in `docs/hardware.md`.
Language: docs, code comments and commit messages are in English. User-facing text in the firmware
and web app exists in English and Dutch (firmware `tr(en, nl)` helper in
`firmware/lib/buddy/src/i18n.h`, web app dictionary in `firmware/web/index.html`); new UI text must
be added in both languages.

## Device
- USB `303a:1001`, port `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_*-if00`
- Access to the port requires the user to be in `dialout`. Never run sudo yourself; ask the user.
  If the group has been added but is not yet active in the shell, use `sg dialout -c "…"`.
- Use the scripts in `tools/` (esptool v5 in `.venv/`, commands with dashes, such as `read-flash`)

## Code
- `firmware/`: PlatformIO, with env `esp32` (device), `sim` (headless PC simulator), `emu`
  (interactive SDL emulator, `tools/emu.sh`) and `webemu` (web emulator with web interface and PWA,
  `tools/webemu.sh` -> http://localhost:8765/emu). Explained in `docs/firmware.md`.
- Two boards: the Original (1.75", round 466x466) and the XL (2.16", square 480x480, `docs/xl-port.md`).
  Every env has an `-xl` twin (`esp32-xl`, `sim-xl`, `emu-xl`, `webemu-xl`) that only adds
  `-D DESKBUDDY_BOARD_XL`; screen size comes from `firmware/lib/buddy/src/screen.h` (never hard-code
  466/233), panel settings per board from `firmware/src/board/pins.h`. The scripts take `--xl`.
- Run `tools/webemu-test.sh` (regression test, `docs/regression-test.md`) after changes to the
  firmware logic or web app, and always before a release or a flash request.
- Test functionality and the web app/PWA in the web emulator first (e.g. via the built-in browser,
  configs `webemu` and `webemu-xl` in `.claude/launch.json`); measure performance on the device.
- Hardware-independent logic belongs in `firmware/lib/buddy/` (LVGL only), so the simulator can run
  it too. Hardware belongs in `firmware/src/board/`.
- Check UI changes with `tools/sim.sh` and `tools/sim.sh --xl` and look at the PNGs in `sim-out/` and
  `sim-out-xl/` before asking to flash.
- Plan and phases are in `docs/plan.md`. Navigation: swipe down = info (Wi-Fi/BLE/sound/skin),
  swipe up = clock.
- Start the emulator detached with `setsid … &` and stop it with `pkill -x program`. Do not use
  `pkill -f`, because it also matches your own shell.

## Rules
- **Never flash, erase or run `write-flash` without explicit permission** and without a backup in
  `backups/`. The chip holds the only copy of the closed-source original firmware.
- `backups/` is in `.gitignore` (IP of the original maker). Never commit or publish dumps.
- Read actions (chip-info, read-flash, monitor) are safe.

## Skill
`.claude/skills/esp-device/` contains the workflow for this device: inspect, back up, flash.
