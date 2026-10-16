# GreenBox Custom-Firmware (v5.0.0)

BLE bleibt **wie im Original** (App 4.x und die HA-Integration funktionieren unveraendert), zusaetzlich: WLAN dauerhaft,
Fallback-AP, WebUI, MQTT und eine konfigurierbare OTA-URL.

| Thema | Verhalten |
|---|---|
| BLE | Name `GreenBox`, Service `0x00FF`, Characteristics `FF01/02/03/05/06` wie Original, zusaetzlich `FF07` (OTA-Status), `FF08` (Info), `FF09` (TZ). Framing/Pruefsumme/Notify-Zyklus siehe `../PROTOCOL.md`. |
| Original-Kompatibilitaet | `fw_byte` = 3 (meldet "2.0.3"): Zeitplan in **UTC**, App/Integration rechnen wie gewohnt. Mit `fw_byte` = 4 gilt lokale Zeit + `FF09`-Zeitzone. |
| Persistenz | Alles (Streifen, Zeitplan, Override, Blinken, WLAN, MQTT ...) liegt im NVS. Das Original hielt den Zeitplan nur im RAM. |
| Zeit | NTP, sobald WLAN steht (kein Neu-Verbinden der App nach Stromausfall noetig). Ohne Zeit: Streifen 1 "atmet" wie im Original. |
| WLAN | STA; Zugang wird beim ersten Start aus der Werks-Firmware (NVS-`eeprom`) uebernommen. Kein/kein funktionierendes WLAN -> AP `GreenBox-Setup` (Standard-Passwort `greenbox`, **aendern**), Captive Portal auf `192.168.4.1`. |
| Hostname | Standard `GreenBox` + erste 4 MAC-Bytes (z. B. `GreenBox8C4B14C8`), eindeutig pro Geraet, aenderbar (`hostname`). |
| WebUI | `http://<hostname>.local/` bzw. IP. Optionales Passwort (Benutzer `admin`). |
| OTA | URL im WebUI / `ota_url`. Standard: `https://github.com/kurim/BerlinGreen-FW/releases/latest/download/firmware.bin`. TLS wird geprueft (eingebettete Root-CAs: ISRG, DigiCert G2, USERTrust, Sectigo, GTS, Amazon). Version-Check: `version.txt` im selben Verzeichnis wie die `.bin` (nur Versionsstring, z. B. `5.0.1`). Taeglich-Auto optional. App-Befehl `U` loest ebenfalls OTA aus. |

## Standard-URL fuer dein Repo setzen
`src/gb.h`, `Cfg::otaUrl`. Release-Assets: `firmware.bin` (= `.pio/build/greenbox/firmware.bin`) und `version.txt`.

## Bauen / Flashen
```bash
~/tools/venv/bin/pio run                 # -> .pio/build/greenbox/firmware.bin
./flash.sh /dev/ttyUSB0                  # UART; NVS (WLAN-Daten) bleibt erhalten
flash.cmd COM3                           # Windows (Python + `pip install esptool`)
# beide akzeptieren optional ein Verzeichnis mit den Release-Dateien (firmware.bin, bootloader.bin, partitions.bin, boot_app0.bin)
```
Partitionstabelle ist identisch zum Werkszustand. Rueckweg: `esptool write_flash 0 berlingreen.bin`.
Erster Flash muss per UART/TC2030 erfolgen (die Werks-OTA akzeptiert nur ihr eigenes Zertifikat).

### Pinbelegung Flash-Header (TC2030)

```
   ●       ●
[6] IO0   [5] RXD
[4] GND   [3] TXD
[2] 3V3   [1] EN
        ●
```

Passender Clip/Adapter an UART-USB-Wandler, kein Anloeten am Chip noetig.

| Pin | Funktion | an UART-Wandler |
|---|---|---|
| 1 | EN (Reset) | - (nur beim Flashen ggf. per Taster/DTR beschalten) |
| 2 | 3V3 | 3V3 (nur wenn die Box nicht separat mit Strom versorgt wird) |
| 3 | TXD (Box -> Wandler) | RXD |
| 4 | GND | GND |
| 5 | RXD (Wandler -> Box) | TXD |
| 6 | IO0 (Boot-Modus) | - (fuer Download-Modus beim Verbinden gegen GND ziehen) |

## MQTT
Aktivieren im WebUI (Host/Port/User/Passwort). Basis `<base>` = Einstellung `mqtt_base`, sonst `greenbox/<mac ohne :>`.

| Topic | Richtung | Inhalt |
|---|---|---|
| `<base>/availability` | Box -> Broker, retained, LWT | `online` / `offline` |
| `<base>/state` | Box -> Broker, retained | JSON (s. u.), bei Aenderung + alle 60 s |
| `<base>/set/<key>` | Broker -> Box | Wert als Klartext |
| `<base>/cmd/ota` | Broker -> Box | `check` oder `start` |
| `<base>/cmd/toggle` | Broker -> Box | beliebig: Override wie Taster umschalten |
| `<base>/cmd/reboot` | Broker -> Box | beliebig |

`set`-Keys: `stripe1|stripe2|stripe3` (0-100), `override` (`on`/`off`/`auto`), `start`, `start_weekend` (`HH:MM` oder `HHMM`),
`duration`, `duration_weekend` (h), `blink` (`0`/`1`), `button` (`0`/`1`, Touch-Taster IO12, Standard an), `button_delta` (1-50), `time` (Unix-UTC), `tz`, `fwbyte` (3/4), `ota_url`, `ota_auto`,
`ota_insecure`, `hostname`, `mqtt_*`, `web_password`, `ap_password`.

State-JSON (Auszug):
```json
{"stripe1":100,"stripe2":100,"stripe3":100,"override":"auto","light_on":true,
 "start":"18:00","start_weekend":"18:00","duration":12,"duration_weekend":0,
 "water":50,"low_water":false,"blink_on_low_water":false,"touch":[5,6,70,60],
 "time_valid":true,"fw":"5.0.0","fw_byte":3,
 "wifi":{"connected":true,"ssid":"...","rssi":-60,"ip":"192.168.1.20","ap":false},
 "ota":{"progress":0,"error":0,"msg":"up to date","latest":"5.0.0"},"mqtt":{...}}
```
Hinweis zu Zeiten: `start*` sind in der Zeitbasis der Box (bei `fw_byte` 3 = UTC). Fuer das HA-Addon: `fw_byte` lesen und wie bisher
Ortszeit <-> UTC umrechnen.

## Pins (aus der Werks-Firmware)
Stripes GPIO 23/22/21 (LEDC 25 kHz, 11 Bit), Wasser Touch GPIO 14/27/13 (unten/mitte/oben). GPIO 12 (IO12, Modul-Pin 14) ist der Touch-Taster: Kupfer-Pad mit Guard-Ring unter dem Gehaeuse, ueber 510 Ohm angebunden — bei manchen Gehaeusen von aussen nicht erreichbar, daher per `button` abschaltbar. Standardmaessig an, Schwelle `button_delta` (Standard 5; am Testgeraet trocken ~83, beruehrt ~23 -> 20-25 ist ein robuster Wert), Live-Werte im WebUI.

"Bei wenig Wasser blinken" (`blink`): kurzes Abdunkeln, wenn der Pegel <= 20 % ist (0 oder 20 %).

## Nicht getestet
Gebaut und als Image validiert, aber **noch nicht auf Hardware getestet**. Erst mit niedriger Helligkeit pruefen.
