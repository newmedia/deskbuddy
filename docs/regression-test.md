# Regression test (web emulator)

`tools/webemu-test.sh` checks in about 2.5 minutes whether the firmware logic still does what it
should, without a device, for both boards: the Original (env `webemu`) and the XL (env `webemu-xl`). The test runs on the [web emulator](firmware.md#web-emulator-test-everything-without-a-device).
That is the same `App` as on the device, with the same web API.

## When to run
- after every change in `firmware/lib/buddy/`, `firmware/web/` or `firmware/src/webemu/`
- always before a release (`tools/release.sh`) and before you ask to flash

The test does not measure performance. You measure that on the device (`perf`).

## Usage

`tools/setup.sh` must have been run once (venv with PlatformIO in `.venv/`). Then:

```bash
tools/webemu-test.sh
```

```bash
tools/webemu-test.sh -v
```

With `-v` you also see every passed check, not just the failures. `--board original` or
`--board xl` tests only one board (about 80 seconds).

What the script does:
1. per board: builds env `webemu` or `webemu-xl` (`pio run -e webemu-xl`)
2. finds a free port and starts the emulator in the background, with its **own, empty state** in a
   temporary folder (via `WEBEMU_STATE`). Your regular emulator settings in
   `firmware/.pio/webemu-state/` stay untouched, and an emulator already running on 8765 is not in
   the way. Next to it a second "peer" emulator starts (its own state too) for the buddies checks:
   the two see each other over the fake ESP-NOW (UDP on localhost)
3. runs `tools/webemu_test.py`, which checks everything over HTTP
4. stops the emulator and cleans up the temporary folder, also on Ctrl+C; then the next board

## Reading the result

```
group           ok  fail
start            2     0
emotions        14     0
actions         16     0
look            21     0
wifi             9     0
ble             11     0
swipes           5     0
sound            2     0
microphone      12     0
language         9     0
buddies         11     0
updates         13     0

[original] 127/127 checks passed in 92 s -- PASSED
```

- **Exit code 0**: all good. **Exit code 1**: at least one check failed.
- A failed check appears above as `FAIL [group] name -- details`, with what did come back.
  A group with failures gets `<--` in the table.
- On a failure, the last lines of the emulator log follow. The full log remains in
  `/tmp/webemu-test.XXXXXX.log` (the path is shown).
- If a group goes down because of an exception (for example invalid JSON), that counts as
  one `exception` failure and the other groups just continue.

## What is checked

| Group | Checks |
|---|---|
| start | emulator reachable; `/emu/screen` returns an image of 466×466 RGB565 |
| emotions | every emotion via `emotion <name>` (reply and `/emu/state`), unknown emotion returns 400 |
| actions | every action via `play <name>`, `blink`, `bounce`, `gesture shake`, shaking via `/emu/input`, unknown action returns 400 |
| look | `look presets` is valid JSON, every character via `look preset`, `look save` / `look delete` of a custom profile, error cases |
| wifi | `/api/wifi/scan` first `scanning:true`, then networks; password `wrong` → `failed` "wrong password"; network `Gone` → "network not found"; correct password → `connected`; empty ssid → 400 |
| ble | first connect → `pair` + `pairCode`; wrong code → 400; correct code → connected; line commands (`emotion happy`, `json`) return reply + `\n`; second connect → `bonded` |
| swipes | swipe down → info, swipe back → face, swipe up → clock, tap → face (`view` in `/emu/state`) |
| sound | after `sound on` + `play whistle`, `/emu/audio` returns real PCM |
| microphone | emulated microphone scenes (clap, double clap, noise, music, speech) give the right reaction; `mic`, `mic on|off` |
| buddies | with the peer emulator: no group by default, a too short code gives 400, both in group `regtest` see each other, frames from another group are rejected, a yawn is contagious and passes, the cooldown prevents a second yawn within 30 s, `group off` |
| language | `lang` shows the current language (default `en`), `lang nl` / `lang en` switch it, an invalid value returns 400, `"lang"` in `/api/status`, a custom look is called "Eigen"/"Custom", the language is preserved after a restart, the web app contains the Dutch dictionary and the EN/NL switch |
| updates | `update check` → `available` with the offered version, `update install` → `installing` with increasing progress, restart with new version, look and custom profile preserved; `POST /update` rejects a small file and installs a file >100 KB |

The lists of emotions and actions come from `/emu/state` (`emotions`, `actions`), which gets them
from the firmware itself. A new emotion or action is therefore tested automatically.

## Adding a check

Everything is in [`tools/webemu_test.py`](../tools/webemu_test.py), standard Python only.

- A group is a function `t_<name>()`. Put it in the list `TESTS` at the bottom. The order matters:
  `updates` comes last, because the emulator restarts during it.
- Within a group: `check("name", condition, "detail on failure")`.
- Helpers: `cmd("line")` returns `(status, json)` of `/api/cmd`; `status()` is `/api/status`;
  `state()` is `/emu/state`; `ble(...)`, `swipe(...)`, `touch(...)` and `wifi_join(...)` for input.
- The App only updates some status after a short poll (Wi-Fi, updates). So use
  `wait_for(lambda: ..., seconds)` instead of comparing directly. Otherwise you get a test that
  sometimes fails.
- If you need information that the API does not give yet, add a field to `/emu/state` in
  `firmware/src/webemu/main.cpp`, like `view`.

Then run the test a few times in a row, to be sure it is stable.

## Problems

| Problem | Solution |
|---|---|
| `pio: No such file` | first `tools/setup.sh` |
| build error | you see the same error with `cd firmware && ../.venv/bin/pio run -e webemu` |
| `emulator reachable` fails | look in the log under the summary; the emulator probably crashes on startup |
| timing errors on a busy machine | the wait times are in `wait_for(...)`; raise them there |
| hangs | Ctrl+C stops the test and the emulator; a leftover process you stop with `pkill -x program` |

You can also start the same emulator by hand with its own state folder:

```bash
WEBEMU_STATE=/tmp/my-state tools/webemu.sh 9000
```
