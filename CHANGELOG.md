# Changelog

Elke versie hieronder wordt een release op GitHub (zie `tools/release.sh` en `docs/firmware.md`).
De tekst onder een kop is de omschrijving van die release. Nieuwste bovenaan.

## v0.2.0

Eerste release die de deskbuddy zelf kan ophalen.

- **Automatische updates**: de deskbuddy kijkt bij het opstarten en elke 12 uur of er een nieuwere
  release op GitHub staat. Standaard meldt hij dat (info-paneel en web-UI, met een knop om te
  installeren); met `update auto` installeert hij zelf. Tijdens installeren kijkt hij verrast en
  loopt er een voortgangsring rond het scherm.
- **Wi-Fi-setup in twee stappen**: QR-code voor het setup-netwerk, daarna een QR-code met de
  setup-link; captive portal op Android, iOS en Windows, en terugkoppeling tijdens het verbinden.
- **Webinterface** op `http://deskbuddy.local`: emoties, uiterlijk, geluid, volume, helderheid,
  slaaptijd, Wi-Fi en firmware bijwerken.
- **Instelbare slaaptijd** (1-120 minuten of nooit).
- Tijd via NTP, met de RTC als reserve.

## v0.1.0

Eerste eigen firmware (niet als release gepubliceerd): procedureel gezicht met emoties, touch- en
bewegingsgebaren, klok, info-paneel, geluidseffecten en twee skins (robot en mascotte).
