#!/usr/bin/env python3
"""Regression test for the deskbuddy firmware via the web emulator (see tools/webemu-test.sh).

Usage: webemu_test.py http://127.0.0.1:<port> [-v]
Expects an emulator with a clean state. Standard library only.
"""
import json
import struct
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

BASE = sys.argv[1].rstrip("/")
VERBOSE = "-v" in sys.argv[2:]
results = []  # (group, name, ok, detail)
group = "?"


def http(path, data=None, method=None, ctype=None, timeout=5):
    """Returns (status, headers, body bytes); connection errors give status 0."""
    req = urllib.request.Request(BASE + path, data=data, method=method)
    if ctype:
        req.add_header("Content-Type", ctype)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, r.headers, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.headers, e.read()
    except (urllib.error.URLError, ConnectionError, TimeoutError, OSError):
        return 0, {}, b""


def jget(path):
    st, _, body = http(path)
    if st != 200:
        raise RuntimeError(f"GET {path} -> {st}")
    return json.loads(body)


def cmd(line):
    st, _, body = http("/api/cmd?c=" + urllib.parse.quote(line))
    try:
        js = json.loads(body)
    except ValueError:
        js = {"ok": False, "reply": body.decode(errors="replace")}
    return st, js


def check(name, ok, detail=""):
    results.append((group, name, bool(ok), detail))
    if VERBOSE or not ok:
        print(f"  {'ok  ' if ok else 'FAIL'} [{group}] {name}" + (f" -- {detail}" if detail and not ok else ""))
    return ok


def wait_for(fn, timeout, step=0.1):
    """Repeats fn() until it returns something truthy; returns the last value."""
    end = time.time() + timeout
    v = None
    while time.time() < end:
        try:
            v = fn()
        except Exception:  # emulator is restarting
            v = None
        if v:
            return v
        time.sleep(step)
    return v


def state():
    return jget("/emu/state")


def status():
    return jget("/api/status")


# ---------------------------------------------------------------------------


def t_start():
    ok = wait_for(lambda: http("/api/status")[0] == 200, 15)
    check("emulator reachable", ok)
    if not ok:
        raise SystemExit(1)
    st, h, body = http("/emu/screen")
    check("/emu/screen returns 466x466 RGB565", st == 200 and len(body) == 466 * 466 * 2, f"{st}, {len(body)} bytes")


def t_emotions():
    s = state()
    check("list of emotions in /emu/state", len(s.get("emotions", [])) >= 10, str(s.get("emotions")))
    for e in s["emotions"]:
        st, js = cmd(f"emotion {e}")
        ok = st == 200 and js.get("ok") and js.get("reply") == f"ok emotion={e}"
        got = wait_for(lambda: state()["emotion"] == e, 2) if ok else False
        check(f"emotion {e}", ok and got, f"{st} {js} / state={state()['emotion']}")
    st, js = cmd("emotion doesnotexist")
    check("unknown emotion gives 400", st == 400 and not js.get("ok"), f"{st} {js}")
    cmd("emotion neutral")


def t_actions():
    s = state()
    check("list of actions in /emu/state", len(s.get("actions", [])) >= 9, str(s.get("actions")))
    for a in s["actions"]:
        st, js = cmd(f"play {a}")
        check(f"play {a}", st == 200 and js.get("reply") == f"ok play={a}", f"{st} {js}")
        time.sleep(0.15)
    for c in ("blink", "bounce", "gesture shake"):
        st, js = cmd(c)
        check(c, st == 200 and js.get("ok"), f"{st} {js}")
    st, _, _ = http("/emu/input?t=gesture&g=shake")
    check("shake via /emu/input", st == 200)
    st, js = cmd("play doesnotexist")
    check("unknown action gives 400", st == 400, f"{st} {js}")
    check("emulator still running", http("/api/status")[0] == 200)


def look_list():
    st, js = cmd("look presets")
    return json.loads(js["reply"]) if st == 200 else {}


def t_look():
    lst = look_list()
    presets = lst.get("presets", [])
    check("look presets returns JSON with presets and profiles", presets and "profiles" in lst, str(lst))
    for p in presets:
        st, js = cmd(f"look preset {p}")
        got = wait_for(lambda: status()["look"] == p, 2) if st == 200 else False
        check(f"look preset {p}", st == 200 and got, f"{st} {js} / look={status()['look']}")
    st, js = cmd("look save testprofile")
    check("look save testprofile", st == 200 and "testprofile" in look_list().get("profiles", []), f"{st} {js} {look_list()}")
    st, js = cmd("look delete testprofile")
    check("look delete testprofile", st == 200 and "testprofile" not in look_list().get("profiles", []), f"{st} {js} {look_list()}")
    st, js = cmd("look delete testprofile")
    check("second delete gives an error", st == 400, f"{st} {js}")
    st, js = cmd("look preset doesnotexist")
    check("unknown character gives 400", st == 400, f"{st} {js}")


def wifi_join(ssid, pw):
    data = urllib.parse.urlencode({"ssid": ssid, "pass": pw}).encode()
    st, _, body = http("/api/wifi", data=data, ctype="application/x-www-form-urlencoded")
    return st, body


def wifi_final():
    def done():
        w = jget("/api/wifi/state")
        return w if w["sta"] not in ("connecting",) else None

    return wait_for(done, 6, 0.2) or {}


def t_wifi():
    first = jget("/api/wifi/scan")
    check("scan: scanning:true first", first.get("scanning") is True, str(first))
    res = wait_for(lambda: (lambda j: j if not j.get("scanning") else None)(jget("/api/wifi/scan")), 5, 0.3) or {}
    names = [n["ssid"] for n in res.get("networks", [])]
    check("scan: networks afterwards", "HomeNetwork" in names and "Gone" in names, str(res))

    st, _ = wifi_join("HomeNetwork", "wrong")
    check("join with password 'wrong' accepted", st == 200)
    check("while connecting: sta=connecting", jget("/api/wifi/state")["sta"] == "connecting")
    w = wifi_final()
    check("wrong password -> failed", w.get("sta") == "failed" and w.get("reason") == "wrong password", str(w))

    wifi_join("Gone", "secret123")
    w = wifi_final()
    check("network 'Gone' -> not found", w.get("sta") == "failed" and w.get("reason") == "network not found", str(w))

    wifi_join("HomeNetwork", "secret123")
    w = wifi_final()
    check("correct password -> connected", w.get("sta") == "connected" and w.get("ip"), str(w))
    check("status wifi=sta", wait_for(lambda: status()["wifi"] == "sta", 3), status()["wifi"])

    st, _ = wifi_join("", "x")
    check("empty ssid gives 400", st == 400)


def wait_restart(timeout=20):
    """Waits until the installation is done and the emulator has restarted."""
    return wait_for(lambda: (lambda s: s if s["update"] != "installing" else None)(status()), timeout, 0.25)


def t_update():
    # Settings that must survive the restart
    presets = look_list()["presets"]
    cmd(f"look preset {presets[-1]}")
    cmd("look save kept")  # also becomes the current look
    keep = status()["look"]
    time.sleep(2)  # settings are written out after 1.5 s
    old = status()["version"]

    http("/emu/input?t=offer&v=v9.8.7")
    st, js = cmd("update check")
    check("update check", st == 200 and js.get("ok"), f"{st} {js}")
    s = wait_for(lambda: (lambda s: s if s["update"] not in ("checking",) else None)(status()), 5) or {}
    check("status available with offered version", s.get("update") == "available" and s.get("latest") == "v9.8.7", str(s))

    st, js = cmd("update install")
    check("update install", st == 200 and js.get("ok"), f"{st} {js}")
    check("status installing", wait_for(lambda: status()["update"] == "installing", 3), str(status()))
    prog = wait_for(lambda: status()["progress"] > 0, 3)
    check("progress increases", prog)
    s = wait_restart() or {}
    check("new version after restart", s.get("version") == "9.8.7", f"was {old}, now {s.get('version')}")
    check("look kept after restart", s.get("look") == keep, f"{s.get('look')} != {keep}")
    check("own profile kept after restart", "kept" in look_list().get("profiles", []))

    # Upload via the web interface (POST /update)
    small = b"x" * 1000
    st, _, _ = http("/update", data=small, method="POST", ctype="application/octet-stream")
    check("POST /update with small file rejected", st == 500, str(st))
    big = b"\xe9" + b"\0" * (150 * 1024)
    st, _, body = http("/update", data=big, method="POST", ctype="application/octet-stream")
    check("POST /update with >100 KB accepted", st == 200 and b"true" in body, f"{st} {body[:80]}")
    check("upload: status installing", wait_for(lambda: status()["update"] == "installing", 3))
    s = wait_restart() or {}
    check("upload: restart, version stays", s.get("version") == "9.8.7" and s.get("update") != "installing", str(s))
    check("upload: look kept", s.get("look") == keep)
    cmd("look delete kept")


def ble(a, body=None, **q):
    qs = urllib.parse.urlencode({"a": a, **q})
    st, _, b = http("/emu/ble?" + qs, data=body, method="POST" if body is not None else None, ctype="text/plain")
    return st, b.decode(errors="replace")


def t_ble():
    check("BLE is on", state()["ble"])
    check("not connected: line gives 400", ble("line", b"status\n")[0] == 400)
    st, b = ble("connect")
    check("first connect -> pair", st == 200 and b == "pair", f"{st} {b!r}")
    code = state()["pairCode"]
    check("pairCode in /emu/state", 100000 <= code <= 999999, str(code))
    st, b = ble("pair", code=(code % 999999) + 1)
    check("wrong code -> 400", st == 400, f"{st} {b!r}")
    check("not connected after wrong code", not state()["bleConnected"])

    ble("connect")
    code = state()["pairCode"]
    st, b = ble("pair", code=code)
    s = state()
    check("correct code -> connected", st == 200 and s["bleConnected"] and s["blePaired"], f"{st} {b!r} {s}")
    st, b = ble("line", b"emotion happy\n")
    check("line command: reply + \\n", st == 200 and b == "ok emotion=happy\n", f"{st} {b!r}")
    st, b = ble("line", b"json\n")
    try:
        ok = st == 200 and b.endswith("\n") and json.loads(b)["emotion"] == "happy"
    except ValueError:
        ok = False
    check("line command json", ok, f"{st} {b[:80]!r}")

    ble("disconnect")
    check("disconnect", not state()["bleConnected"])
    st, b = ble("connect")
    check("second connect -> bonded", st == 200 and b == "bonded" and state()["bleConnected"], f"{st} {b!r}")
    ble("disconnect")
    cmd("emotion neutral")


def touch(down, x, y):
    http(f"/emu/input?t=touch&down={1 if down else 0}&x={x}&y={y}")


def swipe(x0, y0, x1, y1, steps=10, ms=250):
    for i in range(steps + 1):
        touch(True, x0 + (x1 - x0) * i // steps, y0 + (y1 - y0) * i // steps)
        time.sleep(ms / steps / 1000)
    touch(False, x1, y1)


def t_swipe():
    check("starts on the face", state()["view"] == "face", state()["view"])
    swipe(233, 120, 233, 360)
    check("swipe down -> info", wait_for(lambda: state()["view"] == "info", 2), state()["view"])
    swipe(233, 360, 233, 120)
    check("swiping back closes info", wait_for(lambda: state()["view"] == "face", 2), state()["view"])
    time.sleep(0.3)
    swipe(233, 360, 233, 120)
    check("swipe up -> clock", wait_for(lambda: state()["view"] == "clock", 2), state()["view"])
    touch(True, 233, 233)  # a tap closes the clock
    time.sleep(0.06)
    touch(False, 233, 233)
    check("back to face", wait_for(lambda: state()["view"] == "face", 2), state()["view"])


def t_audio():
    st, js = cmd("sound on")
    check("sound on", st == 200 and state()["sound"], f"{st} {js}")
    _, h, _ = http("/emu/audio")
    pos = int(h["X-Pos"])
    cmd("play whistle")
    time.sleep(0.6)
    st, h, body = http(f"/emu/audio?pos={pos}")
    n = len(body) // 2
    samples = struct.unpack(f"<{n}h", body[: n * 2]) if n else ()
    peak = max((abs(s) for s in samples), default=0)
    check("/emu/audio returns PCM after play whistle", st == 200 and n > 4000 and peak > 500, f"{n} samples, peak {peak}")


def t_mic():
    """Simulated microphone: every test scene must trigger the right reaction (and speech none)."""
    st, js = cmd("mic")
    check("mic status", st == 200 and js["reply"].startswith("mic=on"), f"{st} {js}")
    check("mic in /api/status", status().get("mic") is True)
    cmd("sound off")  # own sound would deafen the microphone for a moment

    def scene(name, secs):
        cmd("emotion neutral")
        time.sleep(1.2)
        http(f"/emu/input?t=sound&v={name}", method="POST")
        seen, dance, end = [], False, time.time() + secs
        listened.clear()
        while time.time() < end:
            s = state()
            if not seen or seen[-1] != s["emotion"]:
                seen.append(s["emotion"])
            dance |= s.get("dancing", False)
            if s.get("listening"):
                listened.append(True)
            time.sleep(0.08)
        return seen, dance

    listened = []
    seen, dance = scene("speech", 9)
    check("speech: listening (ear), no emotion or dancing", seen == ["neutral"] and not dance and listened,
          f"{seen} listening={bool(listened)}")
    ok = wait_for(lambda: not state().get("listening"), 6)
    check("listening stops after speech", ok)
    seen, _ = scene("clap", 2)
    check("clap: startle (and not listening)", "surprised" in seen and not listened, str(seen))
    seen, _ = scene("double", 2.5)
    check("double clap: happy (without startling first)", "happy" in seen and "surprised" not in seen, str(seen))
    seen, _ = scene("loud", 4)
    check("noise: squeeze eyes shut (pain)", "pain" in seen, str(seen))
    seen, dance = scene("music", 8)
    check("music: dancing", dance and "happy" in seen and "pain" not in seen, f"{seen} dancing={dance}")
    ok = wait_for(lambda: not state().get("dancing"), 10)
    check("dancing stops after the music", ok)
    # mic off while listening: listening must stop
    http("/emu/input?t=sound&v=speech", method="POST")
    wait_for(lambda: state().get("listening"), 6)
    cmd("mic off")
    check("mic off ends listening", wait_for(lambda: not state().get("listening"), 2))
    seen, _ = scene("clap", 2)
    check("mic off: no reaction to a clap", seen == ["neutral"], str(seen))
    check("mic off in /api/status", status().get("mic") is False)
    cmd("mic on")


def t_language():
    check("default language is English", status().get("lang") == "en", str(status().get("lang")))
    st, js = cmd("lang")
    check("lang shows the current language", st == 200 and js.get("reply") == "lang=en", f"{st} {js}")
    st, js = cmd("lang xx")
    check("unknown language gives 400", st == 400, f"{st} {js}")
    st, js = cmd("lang nl")
    check("lang nl", st == 200 and js.get("reply") == "ok lang=nl" and status().get("lang") == "nl", f"{st} {js}")
    st, js = cmd("look color primary #123456")
    check("custom look gets a Dutch name", st == 200 and status().get("look") == "Eigen", status().get("look"))
    time.sleep(2)  # settings are written out after 1.5 s
    cmd("reboot")
    time.sleep(1.5)
    s = wait_for(status, 15)
    check("language kept after restart", s and s.get("lang") == "nl", str(s and s.get("lang")))
    st, js = cmd("lang en")
    check("back to English", st == 200 and status().get("lang") == "en", f"{st} {js}")
    cmd("look color primary #6ee7f7")
    check("custom look gets an English name", status().get("look") == "Custom", status().get("look"))
    _, _, page = http("/")
    html = page.decode(errors="replace")
    check("web app has the Dutch dictionary and the switch", "const NL = {" in html and 'data-lang="nl"' in html)
    time.sleep(2)


TESTS = [
    ("start", t_start),
    ("emotions", t_emotions),
    ("actions", t_actions),
    ("look", t_look),
    ("wifi", t_wifi),
    ("ble", t_ble),
    ("swipes", t_swipe),
    ("sound", t_audio),
    ("microphone", t_mic),
    ("language", t_language),
    ("updates", t_update),
]


def main():
    global group
    t0 = time.time()
    for g, fn in TESTS:
        group = g
        try:
            fn()
        except Exception as e:  # a crash in a group counts as a failure, the rest carries on
            check("exception", False, f"{type(e).__name__}: {e}")
    print()
    print(f"{'group':<12} {'ok':>5} {'fail':>5}")
    for g, _ in TESTS:
        rs = [r for r in results if r[0] == g]
        bad = sum(1 for r in rs if not r[2])
        print(f"{g:<12} {len(rs) - bad:>5} {bad:>5}{'  <--' if bad else ''}")
    bad = [r for r in results if not r[2]]
    print(f"\n{len(results) - len(bad)}/{len(results)} checks passed in {time.time() - t0:.0f} s"
          + (" -- PASSED" if not bad else f" -- {len(bad)} FAILED"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
