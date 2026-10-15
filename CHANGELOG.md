# Changelog

Elke versie hieronder wordt een release op GitHub (zie `tools/release.sh` en `docs/firmware.md`).
De tekst onder een kop is de omschrijving van die release. Nieuwste bovenaan.

## v0.3.2

Zuiniger: de buddy tekent alleen nog als er iets beweegt, en updates lukken ook vanuit de slaapstand.

- **Veel minder CPU-gebruik** (van ~95% naar ~40% als hij wakker is en stilstaat): er wordt niets
  getekend als er niets beweegt, de ademhaling verspringt per hele pixel, en het gezicht wordt niet
  hertekend als de klok of het info-paneel eroverheen ligt.
- **Slaperig en slapend op laag pitje** (~10 en ~8 beelden per seconde), wat de accu spaart.
- **Updates vanuit de slaapstand**: bij het starten van een update wordt de buddy eerst wakker.
  Daarvóór liepen uploads vanuit slaap vast.
- **Touch** wordt alleen nog uitgelezen als het scherm wordt aangeraakt.
- Nieuw commando `reboot` (herstarten op afstand).

## v0.3.1

Updates en Bluetooth gaan nu samen, plus nieuwe acties en een snellere schermaansturing.

- **Updates met Bluetooth aan**: tijdens een update stopt BLE alleen met adverteren in plaats van
  helemaal uit te gaan. In v0.3.0 kon de buddy daardoor zijn Wi-Fi-verbinding kwijtraken; zet
  bij v0.3.0 dus eerst BLE of updates uit, of installeer deze versie handmatig.
- **Meer vrij werkgeheugen met Bluetooth aan** (van een paar KB naar ruim 50 KB), zodat de web-
  interface en updates stabiel blijven.
- **Eigen schermaansturing**: sneller tekenen via asynchrone DMA; de mascotte hertekent in rust
  alleen wat beweegt.
- **Nieuwe acties**: niezen, lachen (3x snel aaien = kietelen), ogen rollen (2x snel heen en weer
  vegen) en de hik, met geluid en knoppen in de webinterface.

## v0.3.0

Bluetooth, een app voor je telefoon en een snellere, zuinigere buddy.

- **Bluetooth (BLE)**: bedienen zonder Wi-Fi via de Nordic UART Service, met hetzelfde
  commandoprotocol. Koppelen met een pincode op het scherm; alleen gekoppelde apparaten krijgen
  toegang. Aan/uit via het info-paneel of `ble on|off|forget`.
- **Web-app (PWA)**: de webinterface is te installeren op je telefoon en werkt via Wi-Fi of via
  Web Bluetooth.
- **Updates naast Bluetooth**: tijdens het controleren en installeren gaat BLE even uit, zodat er
  genoeg geheugen is voor de beveiligde verbinding met GitHub. Een automatische controle wacht
  als er een telefoon verbonden is.
- **Matrix-animatie tijdens updates**: groene ogen met vallende code en een voortgangsring, ook
  bij uploaden via de webpagina en ArduinoOTA.
- **Sneller en zuiniger**: veel meer vrij werkgeheugen, firmware 160 KB kleiner, scherpere klok,
  snellere schermaansturing en vlottere webinterface (geen Wi-Fi-slaapstand).
- Nieuwe commando's: `reboot`, `perf`, `json` en `wifi scan`; `wifi join` accepteert netwerknamen
  met spaties tussen aanhalingstekens.

## v0.2.0

Eerste release die de deskbuddy zelf kan ophalen.

- **Automatische updates**: de deskbuddy kijkt bij het opstarten en elke 12 uur of er een nieuwere
  release op GitHub staat. Standaard meldt hij dat (info-paneel en web-UI, met een knop om te
  installeren); met `update auto` installeert hij zelf. Tijdens installeren kijkt hij verrast en
  loopt er een voortgangsring rond het scherm.
- **Acties met geluid**: fluiten (spontaan bij verveling), knipoog (dubbeltik), kusje (5x aaien)
  en bij herhaald tikken boos, dan rage en tot slot ontploffen. Ook via `play` en de web-UI.
- **Wi-Fi-setup in twee stappen**: QR-code voor het setup-netwerk, daarna een QR-code met de
  setup-link; captive portal op Android, iOS en Windows, en terugkoppeling tijdens het verbinden.
- **Webinterface** op `http://deskbuddy.local`: emoties, uiterlijk, geluid, volume, helderheid,
  slaaptijd, Wi-Fi en firmware bijwerken.
- **Instelbare slaaptijd** (1-120 minuten of nooit).
- Tijd via NTP, met de RTC als reserve.

## v0.1.0

Eerste eigen firmware (niet als release gepubliceerd): procedureel gezicht met emoties, touch- en
bewegingsgebaren, klok, info-paneel, geluidseffecten en twee skins (robot en mascotte).
