# Changelog

Elke versie hieronder wordt een release op GitHub (zie `tools/release.sh` en `docs/firmware.md`).
De tekst onder een kop is de omschrijving van die release. Nieuwste bovenaan.

## v1.1.1

Het deskbuddy-logo staat nu overal: op het scherm van de buddy, in de web-app en op de installatiepagina.

- **Opstartscherm**: bij het inschakelen toont de buddy kort het logo met zijn naam en versie, en
  gaat daarna zacht over in het gezicht.
- **Info-paneel**: een klein logo naast de versieregel.
- **Web-pagina op het apparaat** (en de PWA): hetzelfde logo in de koptekst en als tabblad-icoon, in
  plaats van de eerdere eigen versie zonder lach.
- **Firmware installeren via USB**: nieuwe sectie in het tabblad Systeem en een voettekst met links naar
  de installatiepagina (https://newmedia.github.io/deskbuddy/flash/), waar je ook een oudere versie kunt
  kiezen.

## v1.1.0

De buddy luistert nu mee: hij reageert op klappen, praten, muziek en lawaai.

- **Microfoon**: de twee ingebouwde microfoons worden nu gebruikt. Er wordt alleen het
  geluidsniveau gemeten; er wordt niets opgenomen of verstuurd.
  - **Klap**: hij schrikt, of wordt wakker als hij sliep.
  - **Dubbele klap**: een blij sprongetje.
  - **Praten**: hij luistert. Om en om steekt links en rechts een oortje uit, de geluidsgolfjes
    komen van buiten naar het oor, en de ogen kijken mee.
  - **Muziek**: hij danst op de maat, met hupjes en om en om opzij kijken.
  - **Hard lawaai**: hij knijpt zijn ogen dicht.
  - Gewone achtergrondruis (ventilator, pc) en zijn eigen geluidjes negeert hij.
  - Aan/uit via de knop "Microfoon" in de web-app (Instellingen) of het commando `mic on|off`.
- **Handmatig updaten**: de web-app verwijst nu naar de releases op GitHub om `firmware.bin` te
  downloaden.
- Voor ontwikkelaars: een web-emulator (`tools/webemu.sh`) en een automatische regressietest
  (`tools/webemu-test.sh`) om alles, ook de web-app en de PWA, zonder apparaat te testen.

## v1.0.0

De eerste volwaardige versie: een eigen uiterlijk, nieuwe emoties, een vernieuwde web-app en een
snellere, zuinigere buddy. Vanaf nu volgen we semver: nieuwe functies verhogen het middelste getal
(1.1.0), alleen bugfixes het laatste (1.0.1).

- **Uiterlijk als bouwdoos**: kies voor robot of cartoon en stel ogen, wenkbrauwen, mond, wangen,
  bril, haar, wimpers, lippenstift, oorbellen en haaraccessoires zelf samen, met eigen kleuren.
  Er zijn 16 kant-en-klare personages (o.a. Lisa, Noor, Sofie, Mila, Emma en Bo) en een
  "verras me". Eigen looks bewaar je onder een naam (tot 8 profielen).
- **Nieuwe emoties**: huilen (tranen over de wangen), druk/ADHD (rondschietende blik en
  sprongetjes) en pijn (grimas, zweetdruppel en af en toe een steek).
- **Stuiteren en trillen echt zichtbaar**: stuiteren is een sprongetje, de hik een schok omhoog, en
  bij rage, ontploffen en pijn trilt het hele gezicht. Daarvóór zag je daar niets van.
- **Vernieuwde web-app**: tabbladen (bediening, instellingen, Wi-Fi, systeem) en een editor voor
  het uiterlijk die je met het palet-icoon aan- en uitzet. De editor heeft knoppen in plaats van
  keuzelijsten en kleurstalen bij elk onderdeel. Bovenaan zie je hoe hij verbonden is (Wi-Fi met
  signaalsterkte, Bluetooth of setup) en of hij op USB-stroom of op de accu draait.
- **Sneller en meer geheugen**: het scherm wordt in één keer getekend in plaats van in 18 stroken,
  waardoor tekenen 30-50% sneller gaat. Er is ~80 KB vrij werkgeheugen met Wi-Fi en Bluetooth aan
  (was ~52 KB), als ruimte voor nieuwe functies.
- **Bugfixes**:
  - knipoog en kusje: het dichte oog was bij de cartoon onzichtbaar;
  - tranen en slaap-Z'jes verdwenen achter lang of donker haar;
  - na het wakker worden bleef soms een Z'je staan;
  - bij het vizier bleef in de slaap een blokje staan.

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
