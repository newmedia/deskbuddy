# deskbuddy-firmware

PlatformIO-project in `firmware/`: Arduino-ESP32 3.3 (pioarduino `55.03.311`) en LVGL 9.5.

## Bediening

| Actie | Reactie |
|---|---|
| Tik | kijkt naar je vinger, blij of verrast; maakt hem wakker |
| Dubbeltik | knipoog |
| Lang indrukken | verliefd (hartjesogen) |
| Aaien (heen en weer vegen) | blij, na 3× verliefd, na 5× een kusje |
| Snel blijven tikken | 5× boos, 9× rage, en doortikken tijdens rage laat hem ontploffen |
| Veeg links/rechts | kijkt die kant op |
| **Veeg omlaag** | **info-paneel**: Wi-Fi (IP/SSID of access point), BLE, geluid en uiterlijk (tik om te wisselen), accu en versie |
| **Veeg omhoog** | **klok** met datum en accuring |
| Tik of terugvegen in een paneel | paneel sluiten (sluit ook vanzelf) |
| Schudden | duizelig, daarna even boos (schudden terwijl hij boos is: rage) |
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
2. Veeg omlaag en tik op de Wi-Fi-rij. **Stap 1:** scan de QR-code om met het setup-netwerk te
   verbinden. **Stap 2:** zodra je telefoon verbonden is, toont het scherm een tweede QR-code met de
   link `http://192.168.4.1`. Op veel telefoons opent de setup-pagina ook vanzelf (captive portal);
   Samsung doet dat niet altijd, en stap 2 werkt overal.
3. Kies je netwerk (alleen 2.4 GHz), vul het wachtwoord in (met "Wachtwoord tonen" kun je het
   controleren) en tik op Verbinden. Onder de knop zie je de voortgang, het IP-adres of de reden van
   een mislukte poging.
4. 30 seconden na een gelukte verbinding schakelt hij over naar je thuisnetwerk. Daar is hij
   bereikbaar via het IP uit het info-paneel, of via **`http://deskbuddy.local`** als je apparaat mDNS
   ondersteunt.

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
| `GET /api/wifi/state` | voortgang: `connecting`/`connected`/`failed` + IP of reden |
| `POST /update` (multipart `firmware`) | firmware bijwerken (OTA) |

Firmware bijwerken zonder kabel kan via de webpagina, of met
`curl -F firmware=@firmware/.pio/build/esp32/firmware.bin http://<ip>/update`,
of met `cd firmware && ../.venv/bin/pio run -e esp32 -t upload --upload-port deskbuddy.local`
(ArduinoOTA). Na verbinding haalt hij de tijd via NTP op en zet die in de RTC.

## Automatische updates (GitHub Releases)

Verbonden met het thuisnetwerk vraagt de deskbuddy 30 s na het verbinden en daarna elke 12 uur
`https://api.github.com/repos/newmedia/deskbuddy/releases/latest` op (HTTPS met de CA-bundel uit
ESP-IDF). Is de tag nieuwer dan de eigen versie (semver), dan haalt hij het asset `firmware.bin`
van die release op en flasht hem met `HTTPUpdate` (de redirect naar de downloadserver van GitHub
wordt gevolgd). Controleren en installeren lopen in een eigen taak; het gezicht blijft bewegen.

Instelling (in NVS, standaard `notify`), via commando of web-UI:

| `update …` | effect |
|---|---|
| `notify` | alleen melden: het info-paneel toont onderaan "Update beschikbaar: vX (tik)", de web-UI een knop |
| `auto` | nieuwe versie meteen installeren |
| `off` | niet zelf controleren |
| `check` / `install` | nu controleren / nu installeren |
| (leeg) | status: modus, toestand, huidige en nieuwste versie, laatste fout |

Tijdens het installeren kijkt het gezicht verrast en loopt er een voortgangsring langs de rand.
Daarna herstart hij met de nieuwe versie; mislukt het, dan kijkt hij even verdrietig en probeert
hij het na 30 min opnieuw te controleren.

### Een release maken

We bouwen zelf, niet met GitHub Actions. `tools/release.sh` doet alles:

1. Schrijf bovenaan `CHANGELOG.md` een sectie `## v0.2.0` met wat er nieuw is (die tekst wordt de
   omschrijving van de release op GitHub). Zonder sectie stopt het script en toont het een concept
   op basis van de commits sinds de vorige tag. Commit de changelog.
2. `tools/release.sh 0.2.0` controleert dat de werkmap schoon is en dat de versie nieuwer is dan de
   laatste tag, bouwt `esp32` met `-D DESKBUDDY_VERSION="0.2.0"`, maakt en pusht tag `v0.2.0`
   (`git tag -a v0.2.0 && git push origin v0.2.0`) en maakt met `gh release create` de release met
   `firmware.bin` en `firmware.bin.sha256`.

`gh` moet schrijfrechten op `newmedia/deskbuddy` hebben via de API (het script controleert dat).
Pushen via SSH is niet genoeg: blokkeert de organisatie de GitHub CLI-app, keur die dan goed onder
*Organization settings → Third-party access*, of gebruik een fine-grained token met
"Contents: read and write" voor deze repo (`GH_TOKEN=… tools/release.sh 0.2.0`). Faalt het uploaden
nadat de tag al gepusht is, dan hervat een tweede run met dezelfde versie op dezelfde commit.

Oude releases blijven altijd staan (het script past bestaande tags of releases nooit aan), zodat je
een oudere versie nog handmatig kunt installeren: download die `firmware.bin` en upload hem via de
web-UI of `curl -F firmware=@firmware.bin http://<ip>/update`. Automatisch gaat hij alleen vooruit,
nooit terug.

> Beveiliging: de webinterface en OTA hebben nog geen wachtwoord. Iedereen op je (thuis)netwerk kan
> hem dus bedienen en bijwerken. Een pincode is gepland.

De pagina staat in `firmware/web/index.html`. Bij het bouwen wordt hij gzip-gecomprimeerd in de
firmware gezet (`scripts/embed_web.py`).

## Bluetooth en PWA (fase 3)

- De deskbuddy adverteert als **`deskbuddy-XXXX`** met de Nordic UART Service (NUS, `6E400001-…`).
  Commando's gaan naar RX (`…0002`, write), en antwoorden komen als notify op TX (`…0003`), in
  stukjes en afgesloten met `\n`. Het protocol is gelijk aan serial/web, plus `json` voor de status.
- **Koppelen met pincode:** bij de eerste verbinding toont het scherm een 6-cijferige code, die je op
  je telefoon invult. Alleen gekoppelde apparaten mogen commando's sturen. `ble forget` wist alle
  koppelingen.
- **De PWA** staat op **https://newmedia.github.io/deskbuddy/**. Hij gebruikt dezelfde pagina
  (`firmware/web/`), maar praat via Web Bluetooth, werkt ook zonder Wi-Fi en is te installeren als
  app ("Toevoegen aan startscherm"). Via Bluetooth kun je ook Wi-Fi instellen: netwerken zoeken,
  verbinden.
  - Werkt in Chrome op Android en desktop. **Niet op iPhone**: Safari heeft geen Web Bluetooth. Gebruik
    daar Wi-Fi via het IP-adres.
  - De versie die het apparaat zelf serveert (http) gebruikt Wi-Fi. Web Bluetooth vereist https,
    daarom staat de BLE-versie op GitHub Pages.
- BLE aan/uit: tik op de Bluetooth-rij in het info-paneel, of gebruik `ble on|off`. De instelling
  wordt bewaard.
- Publiceren gaat **zonder GitHub Actions**: `tools/publish-pwa.sh` zet de bestanden uit
  `firmware/web/` op de branch `gh-pages`. Draai dit script na elke wijziging aan de pagina.
  Eenmalig nodig: GitHub → Settings → Pages → Source: **Deploy from a branch**, branch `gh-pages`,
  map `/ (root)`.

## Acties

Korte animaties bovenop de emotie (`face_fx.cpp`). Na afloop keert hij terug naar de vorige emotie.

| Actie | Commando | Beeld | Geluid |
|---|---|---|---|
| Fluiten | `play whistle` | getuite mond, muzieknootjes, halfdichte ogen (ook spontaan bij verveling) | melodietje |
| Knipoog | `play wink` | één oog dicht, sterretje | "ting" |
| Kusje | `play kiss` | ogen dicht, getuite lippen, hartje vliegt naar voren | smak |
| Rage | `play rage` | rode gloed, trillen, stoomwolkjes, `#@!$%` | grom + sis |
| Ontploffen | `play explode` | opzwellen, witte flits, vonken, daarna roet, rook en duizelig/verbaasd | lont-gesis + boem |

Het engine-deel bevat:
- sluiting per oog
- een deeltjessysteem (noten, harten, stoom, rook, vonken, symbolen, sterren)
- trillen (het hele gezichtsobject verschuift)
- tint-, flits- en roetlagen

Elke actie zet *cues*, waar de App een geluid aan koppelt. Ze werken in beide skins.

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
play <whistle|wink|kiss|rage|explode>
look <x> <y> | look off      # -1..1
blink | bounce
gesture <tap|double_tap|long_press|pet|swipe_left|...|shake|pick_up>
brightness <0-255>
sound on|off|<blip|happy|love|surprised|sad|angry|dizzy|yawn|wake|purr|toggle>
volume <0-100>
skin classic|mascot
wifi status | setup | on | off | forget | scan | join "<ssid>" <wachtwoord>
ble on | off | forget | status
json                          # status als JSON (voor BLE-clients)
update [check|install|auto|notify|off]
clock | info | status | help
```

## Opbouw

| Pad | Inhoud |
|---|---|
| `lib/buddy/` | Hardware-onafhankelijk (alleen LVGL): `face` en `face_mascot` (procedureel gezicht, twee skins), `face_fx` (acties en effecten), `sound` (synth), `behaviour` (emoties en verveling), `gestures` (touch en IMU), `clock_view`, `info_view`, `commands`, `app` (koppelt alles en regelt de navigatie) |
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
| F / N / J / R / E | fluiten / knipoog / kusje / rage / ontploffen |
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
