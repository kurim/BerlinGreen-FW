# BerlinGreen GreenBox — Firmware

Reverse Engineering und zwei Firmware-Varianten für die [BerlinGreen GreenBox](https://www.berlingreen.com/)
(ESP32, 3 LED-Streifen, kapazitive Touch-Sensoren für Wasserstand und Taster, BLE-Steuerung über die
BerlinGreen-App).

> **Kein offizielles BerlinGreen-Projekt.** Entstanden aus eigenem Reverse Engineering des Geräts, auf eigene
> Gefahr und ohne jede Garantie. Markenname nur zur Beschreibung der Kompatibilität verwendet.

## Varianten

| | [`firmware/`](firmware/) | [`esphome/`](esphome/) |
|---|---|---|
| Basis | Eigene PlatformIO-Firmware (Arduino-ESP32 2.0.14) | ESPHome-YAML |
| BLE (App-Kompatibilität) | ✅ vollständig (Service `0x00FF`, `FF01`–`FF09`) | ❌ nicht enthalten |
| WLAN + Fallback-AP | ✅ | ✅ |
| WebUI | ✅ eigene Seite, inkl. Live-Ansicht der Touch-Sensoren | ✅ (ESPHome-Standard-UI) |
| MQTT | ✅ eigenes Schema für Home Assistant | ✅ ESPHome-`api`/MQTT |
| HTTPS-OTA mit eigener URL | ✅ | über ESPHome/Dashboard |

Empfehlung: `firmware/`, wenn die BerlinGreen-App weiterhin funktionieren soll. `esphome/`, wenn du ausschließlich
über Home Assistant steuerst und auf die App verzichtest.

Details, Pin-Belegung, MQTT-Topics, Build- und Flash-Anleitung: [`firmware/README.md`](firmware/README.md).
Reverse-Engineering-Hintergrund (Protokoll, GPIOs, Original-Logik, Ghidra-Vorgehen): [`BerlinGreen_GreenBox_RE.md`](BerlinGreen_GreenBox_RE.md).
Änderungen je Version: [`Changelog.md`](Changelog.md).

## Wichtig: Das Gerät muss einmal geöffnet werden

Es gibt **keinen Weg, die Custom-Firmware ohne Öffnen des Geräts aufzuspielen.** Die Original-Firmware prüft beim
OTA-Update ein fest einprogrammiertes, selbstsigniertes Zertifikat, das ausschließlich Hersteller-Updates von
`upotaspre.berlingreen.com` akzeptiert — eigene Firmware kann darüber nicht eingespielt werden.

Der **erste** Flash muss deshalb per UART/TC2030-Pogo-Pins auf der Platine erfolgen (siehe
[`BerlinGreen_GreenBox_RE.md`](BerlinGreen_GreenBox_RE.md) für die Pin-Lage, [`firmware/flash.sh`](firmware/flash.sh)
bzw. [`firmware/flash.cmd`](firmware/flash.cmd) für den Ablauf). Dabei bleibt der NVS-Bereich (WLAN-Zugangsdaten der
Werks-Firmware) unangetastet — die Custom-Firmware übernimmt sie beim ersten Start automatisch.

**Erst nach diesem einen UART-Flash** übernimmt die Custom-Firmware die OTA-Rolle: Sie prüft reguläre TLS-Zertifikate
(öffentliche Root-CAs, eingebettet) gegen eine konfigurierbare URL — standardmäßig die
[Releases dieses Repos](../../releases/latest). Ab da sind weitere Updates kabellos möglich.

Rückweg zur Werks-Firmware jederzeit: `esptool write_flash 0 berlingreen.bin` (eigener Dump, nicht Teil dieses Repos).

## Geräte-Identifikation

Standard-Hostname und mDNS-Name sind `GreenBox` + die ersten 4 Bytes der MAC-Adresse in Großschreibung, z. B.
`GreenBox8C4B14C8` — eindeutig pro Gerät, erreichbar unter `http://GreenBox8C4B14C8.local/`. Anpassbar im WebUI
oder per MQTT (`hostname`).

## Status

Beide Varianten sind gebaut/validiert; der Praxistest läuft gerade auf echter Hardware (siehe
[`Changelog.md`](Changelog.md)). Erste Inbetriebnahme mit niedriger Helligkeit empfohlen, Standard-AP-Passwort
(`greenbox`) und WebUI-Passwort nach der Einrichtung ändern.

## Lizenz

Siehe [`LICENSE`](LICENSE). Bezieht sich nur auf den Code in diesem Repo — nicht auf BerlinGreen-Firmware, -App
oder -Hardware.
