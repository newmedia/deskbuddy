# deskbuddy-firmware

PlatformIO-project in `firmware/`: Arduino-ESP32 3.3 (pioarduino `55.03.311`) en LVGL 9.5.

## Bediening

| Actie | Reactie |
|---|---|
| Tik | kijkt naar je vinger, blij of verrast; maakt hem wakker |
| Dubbeltik | heel blij + stuiter |
| Lang indrukken | verliefd (hartjesogen) |
| Aaien (heen en weer vegen) | blij, na 3× verliefd |
| Veeg links/rechts | kijkt die kant op |
| **Veeg omlaag** | **info-paneel**: Wi-Fi (IP/SSID of access point), BLE, geluid en uiterlijk (tik om te wisselen), accu en versie |
| **Veeg omhoog** | **klok** met datum en accuring |
| Tik of terugvegen in een paneel | paneel sluiten (sluit ook vanzelf) |
| Schudden | duizelig, daarna even boos |
| Oppakken | verrast |
| Kantelen | ogen kijken "bergafwaarts" |
| Niets doen | na 90 s slaperig en na 3 min in slaap (scherm dimt) |
| PWR-knop (kort) | info-paneel aan/uit |
| BOOT-knop | emoties doorlopen (om te testen) |

Wi-Fi en BLE zijn optioneel. Zonder verbinding werkt alles hierboven gewoon, en het info-paneel
toont dan "niet verbonden" en "uit". BLE volgt in fase 3.

## Wi-Fi en webinterface (fase 2)

1. Zonder bekend netwerk start hij het open setup-netwerk **`deskbuddy-XXXX`** (de laatste 4 tekens
   van het MAC-adres). Na 10 minuten zonder bezoekers gaat het weer uit.
2. Veeg omlaag en tik op de Wi-Fi-rij. Er verschijnt een **QR-code**: scan die met je telefoon, dan
   opent de setup-pagina vanzelf (captive portal). Lukt dat niet, ga dan naar `http://192.168.4.1`.
3. Kies je netwerk, vul het wachtwoord in en tik op Verbinden. Het info-paneel toont daarna het IP.
4. Daarna is hij bereikbaar op **`http://deskbuddy.local`** (of via het IP).

De webinterface biedt:
- emoties, knipperen, stuiteren en schudden
- skin, geluid, volume en helderheid
- status en Wi-Fi (zoeken, verbinden, vergeten)
- een firmware-upload
- een commandoregel

| API | Doel |
|---|---|
| `GET /api/status` | status als JSON |
| `POST /api/cmd` (`c=<commando>`) | elk serial-commando, antwoord als JSON |
| `GET /api/wifi/scan` | netwerken zoeken (asynchroon: herhaal tot `scanning:false`) |
| `POST /api/wifi` (`ssid`, `pass`) | verbinden en opslaan |
| `POST /update` (multipart `firmware`) | firmware bijwerken (OTA) |

Firmware bijwerken zonder kabel kan via de webpagina (`firmware/.pio/build/esp32/firmware.bin`),
of met `cd firmware && ../.venv/bin/pio run -e esp32 -t upload --upload-port deskbuddy.local`
(ArduinoOTA). Na verbinding haalt hij de tijd via NTP op en zet die in de RTC.

> Beveiliging: de webinterface en OTA hebben nog geen wachtwoord. Iedereen op je (thuis)netwerk kan
> hem dus bedienen en bijwerken. Een pincode is gepland.

De pagina staat in `firmware/web/index.html`. Bij het bouwen wordt hij gzip-gecomprimeerd in de
firmware gezet (`scripts/embed_web.py`).

## Uiterlijk (skins)

| Skin | Beschrijving |
|---|---|
| `classic` (Robot) | cyaan robotogen op zwart, AMOLED-zuinig (zwarte pixels staan uit) |
| `mascot` (Mascotte) | cartoongezicht met stekelig blauw haar, dikke wenkbrauwen, zwarte bril, witte ogen met blauwe iris en een grote open lach |

Beide skins gebruiken dezelfde emoties, veren en animaties. De mascotte tekent bijna het hele scherm
opnieuw (haar, wenkbrauwen, mond), dus de framerate op het apparaat is nog te controleren. Kiezen
kan in het info-paneel, met `skin mascot` of met toets K in de emulator. De keuze wordt bewaard.

## Geluid

Korte robotgeluidjes worden live berekend (`lib/buddy/sound.*`, 16 kHz). Er zijn dus geen
geluidsbestanden nodig. Er is een geluid voor blij, verliefd, verrast, verdrietig, boos, duizelig,
geeuwen, wakker worden, aaien (spinnen) en tikken. Op het apparaat gaat het geluid via de
ES8311-codec en I2S naar de speaker op MX1.25. De versterker (GPIO46) staat alleen aan tijdens
het afspelen. Je zet het geluid **aan of uit in het info-paneel** (veeg omlaag, tik op "Geluid").
Geluid, volume, helderheid en skin worden in NVS bewaard.

> De ES8311-init (`src/board/audio.cpp`) is een eigen, minimale registerreeks. Die is nog niet op
> het apparaat getest.

## Serial-commando's (115200 baud)

Hetzelfde commandoprotocol wordt later via WebSocket, BLE en MQTT gebruikt.

```
emotion <neutral|happy|sad|angry|surprised|sleepy|love|dizzy|asleep> [ms]
look <x> <y> | look off      # -1..1
blink | bounce
gesture <tap|double_tap|long_press|pet|swipe_left|...|shake|pick_up>
brightness <0-255>
sound on|off|<blip|happy|love|surprised|sad|angry|dizzy|yawn|wake|purr|toggle>
volume <0-100>
skin classic|mascot
wifi status | setup | on | off | forget | join <ssid> <wachtwoord>
clock | info | status | help
```

## Opbouw

| Pad | Inhoud |
|---|---|
| `lib/buddy/` | Hardware-onafhankelijk (alleen LVGL): `face` en `face_mascot` (procedureel gezicht, twee skins), `sound` (synth), `behaviour` (emoties en verveling), `gestures` (touch en IMU), `clock_view`, `info_view`, `commands`, `app` (koppelt alles en regelt de navigatie) |
| `src/board/` | Hardware: CO5300-display via QSPI, CST9217-touch, QMI8658-IMU, AXP2101-PMU en PCF85063-RTC |
| `src/main.cpp` | Apparaat: polling-loop, serial-commando's, knoppen |
| `src/sim/` | Headless PC-simulator die dezelfde `App` draait (PNG's, GIF en MP4 met geluid) |
| `src/emu/` | Interactieve emulator (SDL2-venster met geluid) |
| `include/lv_conf.h` | LVGL-configuratie (RGB565, fonts Montserrat 14/20/28/48) |

### Gezicht
De ogen zijn afgeronde rechthoeken. Elke emotie is een *pose*: oogmaat, hoekradius, boven- en
onderooglid met kanteling, kleur, blosjes, mond en hartvorm. Alle waarden lopen via veren naar het
doel, en daardoor vloeien emoties in elkaar over. Daar bovenop zitten knipperen, rondkijken,
ademen, perspectief bij opzij kijken en squash & stretch. Een vormwissel (bijvoorbeeld naar
hartjes) gebeurt achter een knipper. Alleen het gebied rond de ogen wordt elk frame opnieuw
getekend.

Een nieuwe emotie toevoegen: zet een waarde in `Emotion` en een regel in `Face::poseFor()`
(`face.cpp`), en voeg de naam toe aan `kNames`.

## Bouwen, previewen en flashen

```bash
tools/setup.sh        # venv met esptool, pyserial en PlatformIO
tools/sim.sh          # simulator -> sim-out/emotions.png, mascot.png, demo.gif, demo.mp4 (met geluid)
tools/emu.sh          # interactieve emulator (eenmalig: sudo apt install libsdl2-dev)
cd firmware && ../.venv/bin/pio run -e esp32   # alleen bouwen
tools/flash.sh        # bouwen + flashen (vraagt bevestiging, eist een backup)
tools/monitor.sh      # serial log en commando's
```

Terug naar de originele firmware gaat met
`esptool --chip esp32s3 write-flash 0 backups/original-firmware-*.bin`.

## Emulator-bediening

Met de muis bedien je het scherm alsof het je vinger is: klik = tik, vasthouden = lang drukken,
heen en weer slepen = aaien, vegen omlaag of omhoog = info of klok.

| Toets | Actie |
|---|---|
| 1–9 | emotie (neutral … asleep) |
| S / P | schudden / oppakken |
| pijltjes | kantelen |
| B | knipperen |
| C / I | klok / info |
| W | Wi-Fi-status doorlopen (uit → verbinden → verbonden → AP) |
| L | BLE aan/uit |
| M | geluid aan/uit |
| K | skin wisselen |
| + / - | accu |
| Esc | stoppen |

## Nog te verifiëren op het echte apparaat
- Geluid: de ES8311-init, het volume en of GPIO46 inderdaad de versterker schakelt
- Of de kantel-assen kloppen (`Behaviour::tiltSignX/Y`) en of de drempels voor schudden en
  oppakken goed zijn
- Framerate. Zo nodig de QSPI-klok verhogen (`gfx->begin(80000000)`) of de buffers in DMA-RAM
  zetten.
- Of touch en kantelen dezelfde kant op werken als het beeld. Het scherm is vast 90° gedraaid
  (`kRotation` in `src/board/board.cpp`), omdat het bord gedraaid in de behuizing zit. Touch en
  IMU draaien mee.
