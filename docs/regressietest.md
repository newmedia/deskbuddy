# Regressietest (web-emulator)

`tools/webemu-test.sh` controleert in ongeveer 30 seconden of de firmware-logica nog doet wat hij
moet doen, zonder apparaat. De test draait op de [web-emulator](firmware.md#web-emulator-alles-testen-zonder-apparaat).
Dat is dezelfde `App` als op het apparaat, met dezelfde web-API.

## Wanneer draaien
- na elke wijziging in `firmware/lib/buddy/`, `firmware/web/` of `firmware/src/webemu/`
- altijd vóór een release (`tools/release.sh`) en vóór je om flashen vraagt

Prestaties meet de test niet. Die meet je op het apparaat (`perf`).

## Gebruik

Eenmalig moet `tools/setup.sh` gedraaid hebben (venv met PlatformIO in `.venv/`). Daarna:

```bash
tools/webemu-test.sh
```

```bash
tools/webemu-test.sh -v
```

Met `-v` zie je ook elke geslaagde controle, niet alleen de fouten.

Wat het script doet:
1. bouwt env `webemu` (`pio run -e webemu`)
2. zoekt een vrije poort en start de emulator op de achtergrond, met een **eigen, lege staat** in
   een tijdelijke map (via `WEBEMU_STATE`). Je gewone emulator-instellingen in
   `firmware/.pio/webemu-state/` blijven onaangeroerd, en een emulator die al draait op 8765 zit
   niet in de weg
3. draait `tools/webemu_test.py`, die alles via HTTP controleert
4. stopt de emulator en ruimt de tijdelijke map op, ook bij Ctrl+C

## De uitkomst lezen

```
groep         goed  fout
start            2     0
emoties         14     0
acties          16     0
uiterlijk       21     0
wifi             9     0
ble             11     0
vegen            5     0
geluid           2     0
updates         13     0

93/93 controles geslaagd in 27 s -- GESLAAGD
```

- **Exitcode 0**: alles goed. **Exitcode 1**: minstens één controle mislukt.
- Een mislukte controle staat erboven als `FOUT [groep] naam -- details`, met wat er wel terugkwam.
  Een groep met fouten krijgt `<--` in de tabel.
- Bij een fout volgen de laatste regels van het emulator-log. Het volledige log blijft staan in
  `/tmp/webemu-test.XXXXXX.log` (het pad staat erbij).
- Gaat een groep onderuit door een uitzondering (bijvoorbeeld ongeldige JSON), dan telt dat als
  één fout `uitzondering` en lopen de andere groepen gewoon door.

## Wat er gecontroleerd wordt

| Groep | Controles |
|---|---|
| start | emulator bereikbaar; `/emu/screen` geeft een beeld van 466×466 RGB565 |
| emoties | elke emotie via `emotion <naam>` (antwoord én `/emu/state`), onbekende emotie geeft 400 |
| acties | elke actie via `play <naam>`, `blink`, `bounce`, `gesture shake`, schudden via `/emu/input`, onbekende actie geeft 400 |
| uiterlijk | `look presets` is geldige JSON, elk personage via `look preset`, `look save` / `look delete` van een eigen profiel, foutgevallen |
| wifi | `/api/wifi/scan` eerst `scanning:true`, dan netwerken; wachtwoord `fout` → `failed` "verkeerd wachtwoord"; netwerk `Weg` → "netwerk niet gevonden"; goed wachtwoord → `connected`; lege ssid → 400 |
| ble | eerste connect → `pair` + `pairCode`; verkeerde code → 400; goede code → verbonden; regelcommando's (`emotion happy`, `json`) geven antwoord + `\n`; tweede connect → `bonded` |
| vegen | veeg omlaag → info, terugvegen → gezicht, veeg omhoog → klok, tik → gezicht (`view` in `/emu/state`) |
| geluid | na `sound on` + `play whistle` levert `/emu/audio` echte PCM |
| updates | `update check` → `available` met de aangeboden versie, `update install` → `installing` met oplopende voortgang, herstart met nieuwe versie, look en eigen profiel bewaard; `POST /update` weigert een klein bestand en installeert een bestand >100 KB |

De lijsten met emoties en acties komen uit `/emu/state` (`emotions`, `actions`), die ze uit de
firmware zelf haalt. Een nieuwe emotie of actie wordt dus vanzelf meegetest.

## Een controle toevoegen

Alles staat in [`tools/webemu_test.py`](../tools/webemu_test.py), alleen standaard-Python.

- Een groep is een functie `t_<naam>()`. Zet hem in de lijst `TESTS` onderaan. De volgorde telt:
  `updates` staat als laatste, omdat de emulator daarbij herstart.
- Binnen een groep: `check("naam", voorwaarde, "detail bij fout")`.
- Hulpjes: `cmd("regel")` geeft `(status, json)` van `/api/cmd`; `status()` is `/api/status`;
  `state()` is `/emu/state`; `ble(...)`, `swipe(...)`, `touch(...)` en `wifi_join(...)` voor invoer.
- De App werkt sommige status pas bij na een korte poll (Wi-Fi, updates). Gebruik daarom
  `wait_for(lambda: ..., seconden)` in plaats van direct te vergelijken. Anders krijg je een test
  die soms faalt.
- Heb je informatie nodig die de API nog niet geeft, voeg dan een veld toe aan `/emu/state` in
  `firmware/src/webemu/main.cpp`, zoals `view`.

Draai de test daarna een paar keer achter elkaar, om zeker te zijn dat hij stabiel is.

## Problemen

| Probleem | Oplossing |
|---|---|
| `pio: No such file` | eerst `tools/setup.sh` |
| bouwfout | dezelfde fout zie je met `cd firmware && ../.venv/bin/pio run -e webemu` |
| `emulator bereikbaar` faalt | kijk in het log onder de samenvatting; de emulator crasht waarschijnlijk bij het opstarten |
| tijdfouten op een drukke machine | de wachttijden staan in `wait_for(...)`; verhoog ze daar |
| hangt | Ctrl+C stopt de test en de emulator; een achtergebleven proces stop je met `pkill -x program` |

Met de hand dezelfde emulator met een eigen staatmap starten kan ook:

```bash
WEBEMU_STATE=/tmp/mijn-staat tools/webemu.sh 9000
```
