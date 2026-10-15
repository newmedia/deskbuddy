# Deskimon-software (Creative Chance)

## Wat het is
- Closed-source firmware, alleen als **kant-en-klaar binary** ("ready to flash") geleverd bij de
  digitale download. De broncode zit er bewust niet bij.
- De download bevat ook 3MF/STL-bestanden voor Orbit en Sprout, accessoires, een bouwhandleiding en
  flash-instructies. Printen kost ongeveer 9 uur en 185 g filament voor beide figuren. Je kunt het
  object met de AMS printen, maar dat hoeft niet.
- Andere figuren in dezelfde reeks: Neo, Poko (panda) en Hopper (konijn). Hoofden, lichamen en
  accessoires zijn onderling uitwisselbaar.

## Functies (volgens de productpagina)
- Geanimeerd gezicht met emoties dat reageert op aanraking
- Klok, timer met alarm en accu-indicator
- Werkt **volledig offline**
- Optionele accu: het bord heeft een Li-ion-aansluiting en de AXP2101 regelt het laden

## Wat de hardware nog meer kan (door Deskimon waarschijnlijk niet gebruikt)
De firmware gebruikt vermoedelijk vooral het display, de touch, de RTC en de PMU, en
mogelijk de IMU voor tik- of schuddetectie. Het bord heeft daarnaast:
- **Wi-Fi/BLE**: bijvoorbeeld voor NTP-tijd, notificaties, Home Assistant of een koppeling met de PC
- **Microfoons + speaker**: spraak, geluiden of wake-word (Waveshare demonstreert AI-chat)
- **microSD**: eigen animaties of assets
- **IMU**: kantelen, schudden en oriëntatie

Dit bord is dus een goede basis voor eigen firmware in het `deskbuddy`-project.

## Belangrijk vóór je zelf iets flasht
De Deskimon-firmware staat alleen op de chip. **Maak eerst een volledige flash-backup** met
`tools/backup-flash.sh`. Zonder backup kun je niet terug, behalve met een nieuwe download van
Creative Chance. De backup is alleen voor eigen gebruik. Verspreid hem niet, want de firmware valt
onder het IP van Creative Chance.

## Openstaande vragen (uitzoeken via serial log of flash-backup)
- Welk framework? Arduino/LVGL of ESP-IDF. Is na te gaan met `esptool image_info` op de backup
  of aan de bootlog.
- Indeling van de partitietabel (OTA? SPIFFS/LittleFS met assets?)
- Gebruikt de firmware Wi-Fi of BLE?
