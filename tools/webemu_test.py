#!/usr/bin/env python3
"""Regressietest voor de deskbuddy-firmware via de web-emulator (zie tools/webemu-test.sh).

Gebruik: webemu_test.py http://127.0.0.1:<poort> [-v]
Verwacht een emulator met een schone staat. Alleen standaardbibliotheek.
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
results = []  # (groep, naam, ok, detail)
group = "?"


def http(path, data=None, method=None, ctype=None, timeout=5):
    """Geeft (status, headers, body-bytes); verbindingsfouten geven status 0."""
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
        print(f"  {'ok  ' if ok else 'FOUT'} [{group}] {name}" + (f" -- {detail}" if detail and not ok else ""))
    return ok


def wait_for(fn, timeout, step=0.1):
    """Herhaalt fn() tot hij iets waars geeft; geeft de laatste waarde terug."""
    end = time.time() + timeout
    v = None
    while time.time() < end:
        try:
            v = fn()
        except Exception:  # emulator herstart even
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
    check("emulator bereikbaar", ok)
    if not ok:
        raise SystemExit(1)
    st, h, body = http("/emu/screen")
    check("/emu/screen geeft 466x466 RGB565", st == 200 and len(body) == 466 * 466 * 2, f"{st}, {len(body)} bytes")


def t_emotions():
    s = state()
    check("lijst met emoties in /emu/state", len(s.get("emotions", [])) >= 10, str(s.get("emotions")))
    for e in s["emotions"]:
        st, js = cmd(f"emotion {e}")
        ok = st == 200 and js.get("ok") and js.get("reply") == f"ok emotion={e}"
        got = wait_for(lambda: state()["emotion"] == e, 2) if ok else False
        check(f"emotion {e}", ok and got, f"{st} {js} / state={state()['emotion']}")
    st, js = cmd("emotion bestaatniet")
    check("onbekende emotie geeft 400", st == 400 and not js.get("ok"), f"{st} {js}")
    cmd("emotion neutral")


def t_actions():
    s = state()
    check("lijst met acties in /emu/state", len(s.get("actions", [])) >= 9, str(s.get("actions")))
    for a in s["actions"]:
        st, js = cmd(f"play {a}")
        check(f"play {a}", st == 200 and js.get("reply") == f"ok play={a}", f"{st} {js}")
        time.sleep(0.15)
    for c in ("blink", "bounce", "gesture shake"):
        st, js = cmd(c)
        check(c, st == 200 and js.get("ok"), f"{st} {js}")
    st, _, _ = http("/emu/input?t=gesture&g=shake")
    check("schudden via /emu/input", st == 200)
    st, js = cmd("play bestaatniet")
    check("onbekende actie geeft 400", st == 400, f"{st} {js}")
    check("emulator draait nog", http("/api/status")[0] == 200)


def look_list():
    st, js = cmd("look presets")
    return json.loads(js["reply"]) if st == 200 else {}


def t_look():
    lst = look_list()
    presets = lst.get("presets", [])
    check("look presets geeft JSON met presets en profiles", presets and "profiles" in lst, str(lst))
    for p in presets:
        st, js = cmd(f"look preset {p}")
        got = wait_for(lambda: status()["look"] == p, 2) if st == 200 else False
        check(f"look preset {p}", st == 200 and got, f"{st} {js} / look={status()['look']}")
    st, js = cmd("look save testprofiel")
    check("look save testprofiel", st == 200 and "testprofiel" in look_list().get("profiles", []), f"{st} {js} {look_list()}")
    st, js = cmd("look delete testprofiel")
    check("look delete testprofiel", st == 200 and "testprofiel" not in look_list().get("profiles", []), f"{st} {js} {look_list()}")
    st, js = cmd("look delete testprofiel")
    check("tweede delete geeft fout", st == 400, f"{st} {js}")
    st, js = cmd("look preset bestaatniet")
    check("onbekend personage geeft 400", st == 400, f"{st} {js}")


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
    check("scan: eerst scanning:true", first.get("scanning") is True, str(first))
    res = wait_for(lambda: (lambda j: j if not j.get("scanning") else None)(jget("/api/wifi/scan")), 5, 0.3) or {}
    names = [n["ssid"] for n in res.get("networks", [])]
    check("scan: daarna netwerken", "Thuisnetwerk" in names and "Weg" in names, str(res))

    st, _ = wifi_join("Thuisnetwerk", "fout")
    check("join met wachtwoord 'fout' geaccepteerd", st == 200)
    check("tijdens verbinden: sta=connecting", jget("/api/wifi/state")["sta"] == "connecting")
    w = wifi_final()
    check("verkeerd wachtwoord -> failed", w.get("sta") == "failed" and w.get("reason") == "verkeerd wachtwoord", str(w))

    wifi_join("Weg", "geheim123")
    w = wifi_final()
    check("netwerk 'Weg' -> niet gevonden", w.get("sta") == "failed" and w.get("reason") == "netwerk niet gevonden", str(w))

    wifi_join("Thuisnetwerk", "geheim123")
    w = wifi_final()
    check("goed wachtwoord -> connected", w.get("sta") == "connected" and w.get("ip"), str(w))
    check("status wifi=sta", wait_for(lambda: status()["wifi"] == "sta", 3), status()["wifi"])

    st, _ = wifi_join("", "x")
    check("lege ssid geeft 400", st == 400)


def wait_restart(timeout=20):
    """Wacht tot de installatie klaar is en de emulator opnieuw gestart is."""
    return wait_for(lambda: (lambda s: s if s["update"] != "installing" else None)(status()), timeout, 0.25)


def t_update():
    # Instellingen die de herstart moeten overleven
    presets = look_list()["presets"]
    cmd(f"look preset {presets[-1]}")
    cmd("look save bewaard")  # wordt ook het huidige uiterlijk
    keep = status()["look"]
    time.sleep(2)  # instellingen worden na 1,5 s weggeschreven
    old = status()["version"]

    http("/emu/input?t=offer&v=v9.8.7")
    st, js = cmd("update check")
    check("update check", st == 200 and js.get("ok"), f"{st} {js}")
    s = wait_for(lambda: (lambda s: s if s["update"] not in ("checking",) else None)(status()), 5) or {}
    check("status available met aangeboden versie", s.get("update") == "available" and s.get("latest") == "v9.8.7", str(s))

    st, js = cmd("update install")
    check("update install", st == 200 and js.get("ok"), f"{st} {js}")
    check("status installing", wait_for(lambda: status()["update"] == "installing", 3), str(status()))
    prog = wait_for(lambda: status()["progress"] > 0, 3)
    check("voortgang loopt op", prog)
    s = wait_restart() or {}
    check("na herstart nieuwe versie", s.get("version") == "9.8.7", f"was {old}, nu {s.get('version')}")
    check("na herstart look bewaard", s.get("look") == keep, f"{s.get('look')} != {keep}")
    check("na herstart eigen profiel bewaard", "bewaard" in look_list().get("profiles", []))

    # Upload via de webinterface (POST /update)
    small = b"x" * 1000
    st, _, _ = http("/update", data=small, method="POST", ctype="application/octet-stream")
    check("POST /update met klein bestand geweigerd", st == 500, str(st))
    big = b"\xe9" + b"\0" * (150 * 1024)
    st, _, body = http("/update", data=big, method="POST", ctype="application/octet-stream")
    check("POST /update met >100 KB geaccepteerd", st == 200 and b"true" in body, f"{st} {body[:80]}")
    check("upload: status installing", wait_for(lambda: status()["update"] == "installing", 3))
    s = wait_restart() or {}
    check("upload: herstart, versie blijft", s.get("version") == "9.8.7" and s.get("update") != "installing", str(s))
    check("upload: look bewaard", s.get("look") == keep)
    cmd("look delete bewaard")


def ble(a, body=None, **q):
    qs = urllib.parse.urlencode({"a": a, **q})
    st, _, b = http("/emu/ble?" + qs, data=body, method="POST" if body is not None else None, ctype="text/plain")
    return st, b.decode(errors="replace")


def t_ble():
    check("BLE staat aan", state()["ble"])
    check("niet verbonden: regel geeft 400", ble("line", b"status\n")[0] == 400)
    st, b = ble("connect")
    check("eerste connect -> pair", st == 200 and b == "pair", f"{st} {b!r}")
    code = state()["pairCode"]
    check("pairCode in /emu/state", 100000 <= code <= 999999, str(code))
    st, b = ble("pair", code=(code % 999999) + 1)
    check("verkeerde code -> 400", st == 400, f"{st} {b!r}")
    check("na verkeerde code niet verbonden", not state()["bleConnected"])

    ble("connect")
    code = state()["pairCode"]
    st, b = ble("pair", code=code)
    s = state()
    check("goede code -> verbonden", st == 200 and s["bleConnected"] and s["blePaired"], f"{st} {b!r} {s}")
    st, b = ble("line", b"emotion happy\n")
    check("regelcommando: antwoord + \\n", st == 200 and b == "ok emotion=happy\n", f"{st} {b!r}")
    st, b = ble("line", b"json\n")
    try:
        ok = st == 200 and b.endswith("\n") and json.loads(b)["emotion"] == "happy"
    except ValueError:
        ok = False
    check("regelcommando json", ok, f"{st} {b[:80]!r}")

    ble("disconnect")
    check("disconnect", not state()["bleConnected"])
    st, b = ble("connect")
    check("tweede connect -> bonded", st == 200 and b == "bonded" and state()["bleConnected"], f"{st} {b!r}")
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
    check("start op het gezicht", state()["view"] == "face", state()["view"])
    swipe(233, 120, 233, 360)
    check("veeg omlaag -> info", wait_for(lambda: state()["view"] == "info", 2), state()["view"])
    swipe(233, 360, 233, 120)
    check("terugvegen sluit info", wait_for(lambda: state()["view"] == "face", 2), state()["view"])
    time.sleep(0.3)
    swipe(233, 360, 233, 120)
    check("veeg omhoog -> klok", wait_for(lambda: state()["view"] == "clock", 2), state()["view"])
    touch(True, 233, 233)  # tik sluit de klok
    time.sleep(0.06)
    touch(False, 233, 233)
    check("terug naar gezicht", wait_for(lambda: state()["view"] == "face", 2), state()["view"])


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
    check("/emu/audio geeft PCM na play whistle", st == 200 and n > 4000 and peak > 500, f"{n} samples, piek {peak}")


def t_mic():
    """Nagebootste microfoon: elke testscène moet de juiste reactie geven (en praten geen)."""
    st, js = cmd("mic")
    check("mic-status", st == 200 and js["reply"].startswith("mic=aan"), f"{st} {js}")
    check("mic in /api/status", status().get("mic") is True)
    cmd("sound off")  # eigen geluid zou de microfoon even doof maken

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
    check("praten: luisteren (oor), geen emotie of dansen", seen == ["neutral"] and not dance and listened,
          f"{seen} luisteren={bool(listened)}")
    ok = wait_for(lambda: not state().get("listening"), 6)
    check("na het praten stopt het luisteren", ok)
    seen, _ = scene("clap", 2)
    check("klap: schrikken (en niet luisteren)", "surprised" in seen and not listened, str(seen))
    seen, _ = scene("double", 2.5)
    check("dubbele klap: blij (zonder eerst te schrikken)", "happy" in seen and "surprised" not in seen, str(seen))
    seen, _ = scene("loud", 4)
    check("lawaai: ogen dichtknijpen (pijn)", "pain" in seen, str(seen))
    seen, dance = scene("music", 8)
    check("muziek: dansen", dance and "happy" in seen and "pain" not in seen, f"{seen} dansen={dance}")
    ok = wait_for(lambda: not state().get("dancing"), 10)
    check("na de muziek stopt het dansen", ok)
    # mic uit tijdens het luisteren: luisteren moet stoppen
    http("/emu/input?t=sound&v=speech", method="POST")
    wait_for(lambda: state().get("listening"), 6)
    cmd("mic off")
    check("mic uit beëindigt het luisteren", wait_for(lambda: not state().get("listening"), 2))
    seen, _ = scene("clap", 2)
    check("mic uit: geen reactie op een klap", seen == ["neutral"], str(seen))
    check("mic off in /api/status", status().get("mic") is False)
    cmd("mic on")


TESTS = [
    ("start", t_start),
    ("emoties", t_emotions),
    ("acties", t_actions),
    ("uiterlijk", t_look),
    ("wifi", t_wifi),
    ("ble", t_ble),
    ("vegen", t_swipe),
    ("geluid", t_audio),
    ("microfoon", t_mic),
    ("updates", t_update),
]


def main():
    global group
    t0 = time.time()
    for g, fn in TESTS:
        group = g
        try:
            fn()
        except Exception as e:  # een crash in een groep telt als fout, de rest gaat door
            check("uitzondering", False, f"{type(e).__name__}: {e}")
    print()
    print(f"{'groep':<12} {'goed':>5} {'fout':>5}")
    for g, _ in TESTS:
        rs = [r for r in results if r[0] == g]
        bad = sum(1 for r in rs if not r[2])
        print(f"{g:<12} {len(rs) - bad:>5} {bad:>5}{'  <--' if bad else ''}")
    bad = [r for r in results if not r[2]]
    print(f"\n{len(results) - len(bad)}/{len(results)} controles geslaagd in {time.time() - t0:.0f} s"
          + (" -- GESLAAGD" if not bad else f" -- {len(bad)} MISLUKT"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
