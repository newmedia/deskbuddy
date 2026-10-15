# Meerdere deskbuddies: op elkaar reageren en samen dingen doen

Status: **idee / ontwerpvoorstel** (2026-10-16). Nog niets gebouwd. Dit document beschrijft wat er
kan, welke techniek daarbij past, en een voorstel voor de volgorde.

## Wat willen we?

Een paar voorbeelden, van simpel naar ambitieus:

| Idee | Wat je ziet |
|---|---|
| **Elkaar zien** | Het info-paneel toont welke buddies in de buurt zijn ("3 buddies in de buurt"). |
| **Aanstekelijk gedrag** | Eén buddy geeuwt, en even later geeuwen de anderen ook. Lacht er één, dan lachen de buren mee. |
| **Kijken naar wie iets doet** | Als een buddy ontploft of niest, kijken de andere ogen die kant op. |
| **Kettingreactie** | Bij een rage van één buddy schrikken de anderen, en daarna ontploffen ze één voor één. |
| **De wave** | Buddies op een rij springen na elkaar op, van links naar rechts. |
| **Koor** | Samen fluiten: elke buddy speelt een andere stem van hetzelfde akkoord, precies tegelijk. |
| **Groeps-pomodoro** | Iedereen focust tegelijk; op hetzelfde moment krijgen alle buddies een pauze-animatie. |
| **Berichtje sturen** | Een tik op je buddy stuurt een hartje of kusje naar de buddy van een collega. |
| **Samen vieren** | Een gelukte deploy of verjaardag (via Home Assistant of een webhook) laat alle buddies tegelijk feesten. |

## Techniek: hoe praten ze met elkaar?

| Optie | Bereik | Snelheid | Voordelen | Nadelen |
|---|---|---|---|---|
| **ESP-NOW** (direct radio, ESP32 ↔ ESP32) | zelfde ruimte/verdieping, ~10–50 m | ~1–5 ms | geen router nodig; werkt naast Wi-Fi; broadcast; heel weinig RAM | alleen dichtbij; zelfde Wi-Fi-kanaal als de router nodig; berichten max ~250 bytes |
| **Wi-Fi LAN** (mDNS + UDP-multicast) | zelfde netwerk | ~5–50 ms | ontdekken via `_deskbuddy._tcp`; meerdere ruimtes | alleen op hetzelfde (thuis/kantoor)netwerk; gastnetwerken blokkeren multicast vaak |
| **MQTT-broker** (bijv. Home Assistant of een cloudbroker) | overal met internet | ~50–300 ms | buddies op verschillende locaties; sluit aan bij fase 5 (HA) | broker nodig; TLS kost intern RAM |
| **BLE** | ~10 m | traag (scannen) | afstand schatten via RSSI ("wie staat er naast me?") | scannen vraagt de observer-rol die we om RAM-redenen uit hebben gezet |

**Voorstel:** combineer twee lagen met **hetzelfde berichtformaat**:

1. **ESP-NOW** voor alles in dezelfde ruimte. Dat is snel genoeg voor kettingreacties en
   gelijktijdige acties, en werkt ook als een buddy geen Wi-Fi heeft.
2. **MQTT** voor buddies op afstand, zoals berichtjes naar collega's of een groepsfeest via Home
   Assistant.

## Berichten (concept)

Klein JSON, gelijk aan de bestaande commando's. Een buddy die iets doet, *meldt* dat (event). Een
buddy die iets moet doen, krijgt een *verzoek* (cmd).

```json
{"v":1,"t":"event","from":"deskbuddy-A45C","name":"action","value":"laugh","seq":812}
{"v":1,"t":"cmd","from":"deskbuddy-A45C","to":"*","cmd":"play wave","at":1792101234567,"group":"team"}
{"v":1,"t":"hello","from":"deskbuddy-B21F","skin":"mascot","pos":{"x":2,"y":0},"name":"Bob"}
```

- **`hello`** (elke paar seconden): ik ben er, met naam, skin en positie. Zo weet elke buddy wie er
  in de buurt is.
- **`event`**: wat er gebeurde (emotie, actie, gebaar). Anderen beslissen zelf of en hoe ze reageren.
- **`cmd` met `at`**: een gezamenlijke actie op een afgesproken tijdstip. Daarmee lopen koor, wave
  en feest synchroon.

## Belangrijke ontwerpkeuzes

- **Groepen en vertrouwen:** buddies reageren alleen op buddies uit hun eigen groep ("team").
  Koppelen gaat via de webinterface met een groepscode of QR. ESP-NOW-berichten worden versleuteld
  met de groepssleutel, zodat een vreemde buddy niet zomaar jouw buddy laat ontploffen.
- **Geen eindeloze kettingreacties:** elk event krijgt een `seq`, een hop-telling en een cooldown.
  Lachen is aanstekelijk, maar niet oneindig heen en weer.
- **Persoonlijkheid:** elke buddy krijgt een paar instellingen, zoals hoe aanstekelijk, hoe snel
  boos en of hij mee wil doen aan groepsacties. Zo reageert niet elke buddy hetzelfde, en dat maakt
  het levendiger.
- **Tijdsynchronisatie:** via NTP lopen de klokken al op een paar ms gelijk. Voor ESP-NOW-groepen
  zonder internet stuurt een "leider" zijn tijd mee, en de anderen corrigeren hun afwijking.
- **Ruimtelijk besef** (voor "kijk naar wie iets doet" en de wave): voorlopig handmatig. In de
  webinterface sleep je de buddies op een bureaukaart (links/rechts). Later eventueel een schatting
  via BLE-signaalsterkte.
- **Geheugen en stroom:** ESP-NOW is licht (een paar KB). MQTT met TLS is zwaarder, maar mbedTLS
  draait al in PSRAM. De radio staat voor Wi-Fi toch al aan, dus de extra stroom is beperkt.

## Voorstel voor de volgorde

1. **Elkaar zien:** ESP-NOW-`hello`, een lijst "buddies in de buurt" in het info-paneel en de
   webinterface, en groepen koppelen met een code.
2. **Aanstekelijk gedrag:** geeuwen, lachen en schrikken; kijken naar wie iets doet (met de
   bureaukaart).
3. **Samen doen:** gesynchroniseerde acties met `at`, zoals koor, wave, feest en groeps-pomodoro.
4. **Op afstand:** MQTT-brug, zodat je berichtjes naar collega's kunt sturen en via Home Assistant
   groepsfeesten kunt starten.
5. **Slimmer:** persoonlijkheden, en buddies die "afspraken" maken (bijv. samen een kwartier stil als
   iemand in een call zit).

## Open vragen

- Staan de buddies vooral **in één ruimte** (dan eerst ESP-NOW) of **verspreid** over huis of kantoor
  (dan eerst MQTT)?
- Wie mag een buddy aansturen: alleen buddies uit je eigen groep, of ook "publieke" berichtjes?
- Moet een groepsactie altijd mogen, of wil je per buddy kunnen instellen "niet storen"?
