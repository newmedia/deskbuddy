# Plan: deskbuddy-firmware en apps

De originele firmware is closed source en kan dus niet worden aangepast. Daarom bouwen we eigen
deskbuddy-firmware vanaf nul op dezelfde hardware (zie `hardware.md`). De backup van de originele
firmware staat in `backups/`; terugzetten kan altijd.

## Gekozen uitgangspunten (2026-10-15)

| Onderwerp | Keuze |
|---|---|
| Firmware | PlatformIO + Arduino-ESP32 3.x + **LVGL 9** |
| Animaties | **Mix**: procedureel getekend gezicht met ogen en emoties als basis, plus sprite-animaties voor speciale momenten |
| Aansturing | Webinterface op het apparaat, BLE, Windows-app en Home Assistant/MQTT |
| Telefoon | **PWA**: dezelfde web-app praat via Wi-Fi of Web Bluetooth. Web Bluetooth werkt niet in Safari op iOS; daar gaat het via Wi-Fi. |
| Desktop | **Tauri** (Rust + dezelfde web-UI), voor Windows en ook Linux |
| Functies | Klok, weer en agenda; reageren op beweging en aanraking; geluid en spraak |
| Flash | Volledig voor deskbuddy, met twee OTA-slots en geen fallback naar de originele firmware op het apparaat |

## Architectuur

```
            ┌───────────── deskbuddy (ESP32-S3) ─────────────┐
            │  Face engine (LVGL 9)  ←  Behaviour/emotie-FSM │
            │      ↑ sprites (LittleFS)      ↑ events        │
            │  Sensors: touch · IMU · RTC · PMU · mic        │
            │  Audio: ES8311 speaker · ES7210 mic            │
            │  Command bus (JSON) ← HTTP/WebSocket · BLE GATT · MQTT
            └───────────────┬───────────────┬────────────────┘
                 Wi-Fi (mDNS deskbuddy.local)│BLE
        ┌───────────────────┴──┐   ┌────────┴─────────┐   ┌──────────────┐
        │ Web-UI / PWA         │   │ Tauri desktop app│   │ Home Assistant│
        │ (geserveerd door ESP)│   │ (PC-status push) │   │ via MQTT disc.│
        └──────────────────────┘   └──────────────────┘   └──────────────┘
```

- **Eén commandoprotocol** (JSON-berichten zoals `{"cmd":"emotion","value":"happy"}`) dat over
  WebSocket, BLE (GATT-characteristic) en MQTT hetzelfde werkt. Alle clients spreken dezelfde taal.
- **Web-UI** wordt gebouwd als statische bestanden en geserveerd vanuit LittleFS. De PWA en de
  Tauri-app gebruiken dezelfde code.
- **Wi-Fi-setup**: eerst via BLE vanuit de PWA, of via een access point met captive portal als
  terugvaloptie.
- **OTA**: firmware en LittleFS bijwerken via de webinterface of de desktop-app.

## Repo-indeling (voorstel)

```
firmware/        PlatformIO-project (src/, lib/, data/ voor LittleFS)
  src/face/      procedureel gezicht, emoties, sprite-player
  src/drivers/   display (CO5300 QSPI), touch, IMU, PMU, RTC, audio
  src/net/       wifi, webserver/ws, ble, mqtt, ota
web/             web-UI + PWA (Vite + TypeScript); de build gaat naar firmware/data/
desktop/         Tauri-app (hergebruikt web/)
protocol/        JSON-schema van de commando's en events (gedeeld)
docs/
tools/
```

## Fases

Navigatie (vast): **veeg omlaag** opent het info-paneel met Wi-Fi (IP/SSID of AP), BLE en accu.
**Veeg omhoog** opent de klok. Wi-Fi en BLE zijn optioneel; zonder verbinding werkt de buddy
offline, maar met minder functies.

1. ✅ **Basis-firmware** (zie `firmware.md`): het display werkt met LVGL 9 op 466×466. Het procedurele gezicht knippert,
   kijkt rond en heeft 6–8 emoties. Touch-gestures (tik, aai, lang drukken) en IMU-events
   (schudden, oppakken, kantelen) geven reacties. Plus klok via de RTC en de accu-indicator.
2. ✅ **Wi-Fi en web** (QR-setup, captive portal, `deskbuddy.local`, web-UI, OTA, NTP): captive-portal-setup, mDNS `deskbuddy.local`, webserver met WebSocket,
   web-UI met de knoppen emotie, bericht, helderheid en volume, NTP-tijd en OTA.
   → *Dit is het MVP.* ✅ Plus automatische updates via GitHub Releases (`tools/release.sh`).
3. ✅ **BLE en PWA** (NUS met pincode-koppeling, PWA op GitHub Pages via Web Bluetooth): GATT-service met hetzelfde protocol, Wi-Fi-provisioning via BLE en een
   installeerbare PWA.
4. **Desktop-app (Tauri)**: verbindt via mDNS of BLE en pusht PC-status, notificaties en
   agenda-items naar de buddy.
5. **Home Assistant/MQTT**: MQTT-discovery, emoties als `select`, sensoren voor accu en
   beweging, en het tonen van HA-waarden op het scherm.
6. **Klok, weer en agenda**: weer via Open-Meteo (geen API-key nodig). De agenda loopt via de
   desktop-app of HA.
7. **Geluid en spraak** (✅ geluidseffecten via de synth, aan/uit in het info-paneel): geluidseffecten via de ES8311, klap- en stemdetectie via de ES7210, en
   later eventueel spraakinteractie met een LLM via de desktop-app of HA.
8. ✅ **Skins**: `classic` (robot) en `mascot` (cartoon met haar, bril en lach). Later
   eventueel meer skins.
9. **Sprite-animaties**: speciale animaties uit LittleFS (bijvoorbeeld een LVGL-image-reeks) voor
   events zoals een verjaardag of een nieuwe melding.

## Risico's en aandachtspunten
- **Prestaties**: 466×466 in 16-bit is ±434 KB per frame. Dat vraagt om LVGL-buffers in PSRAM,
  gedeeltelijke updates en DMA via QSPI. Mikken op 30+ fps voor het gezicht.
- Wi-Fi en BLE tegelijk is mogelijk op de S3, maar kost RAM. Daarom NimBLE gebruiken in plaats
  van Bluedroid.
- Voor flashen is toestemming nodig (zie `CLAUDE.md`). De eerste keer flashen overschrijft de
  originele firmware; de backup staat klaar.

## Meerdere buddies (idee)

Op elkaar reageren en samen dingen doen (aanstekelijk gedrag, koor, wave, berichtjes naar collega's):
zie `multi-buddy.md` voor het ontwerpvoorstel (ESP-NOW lokaal + MQTT op afstand).

## Animaties (✅ fluiten, knipoog, kusje, rage en ontploffen gebouwd; de rest staat nog open)

Speciale animaties zijn *acties*: een korte tijdlijn bovenop de gewone emotie. Ze krijgen een eigen
geluid en gaan daarna terug naar de vorige emotie. Ze werken in beide skins en zijn te starten via
een commando (`play <naam>`), de webinterface, BLE en HA, en sommige ook via gebaren.

| Actie | Beeld | Geluid | Trigger |
|---|---|---|---|
| Fluiten | getuite mond, muzieknootjes zweven omhoog, ogen half dicht | melodietje (synth) | spontaan bij verveling |
| Knipoog | één oog dicht (los ooglid per oog), lichte kanteling van het hoofd | "ting" | dubbeltik op één oog |
| Kusje | getuite lippen, hartje vliegt naar voren en groeit | smakje | lang aaien |
| Boos | rode tint, wenkbrauwen/oogleden schuin, korte trilling | brom | herhaald tikken |
| Rage | dieprood, scherm trilt, stoomwolkjes uit de zijkanten, "#@!"-symbooltjes | grommen + sissen | nog vaker tikken of schudden als hij al boos is |
| Ontploffen | opzwellen, witte flits, deeltjes vliegen weg, daarna verkoold en verbaasd (zwarte vegen, rookpluim) | "boem" (ruis-burst) | rage die blijft doorgaan |
| Niezen | ogen dicht, opbouw "ah-ah-", nies-schok | "hatsjoe" | spontaan |
| Lachen | ogen "^ ^", hele gezicht schudt ritmisch | "hihihi"-chirps | kietelen (snel aaien) |
| Rollende ogen | ogen draaien een rondje omhoog | zucht | veeg heen en weer |

Benodigdheden in de engine: per-oog-overrides (knipoog), een deeltjessysteem (noten, hartjes,
stoom, explosie), schermtrilling (offset op alles) en tint/flits-overlays, plus een `Action`-tijdlijn
met keyframes per parameter. De synth krijgt ruis (explosie, sissen) en melodieën (fluiten).
