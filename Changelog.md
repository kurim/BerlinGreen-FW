# Changelog

Alle nennenswerten Änderungen an der GreenBox-Firmware. Format orientiert sich an
[Keep a Changelog](https://keepachangelog.com/de/1.1.0/), Versionierung nach [SemVer](https://semver.org/lang/de/).

## [Unreleased]

## [5.0.3] - 2026-10-16

### Hinzugefügt
- WebUI: Speicher-Bestätigung (Toast) nach jedem "Speichern"-Klick (Zeitplan, OTA, MQTT, System), damit erkennbar
  ist, ob das Setzen tatsächlich geklappt hat
- WebUI: eigenes Favicon statt Browser-Standard

## [5.0.2] - 2026-10-15

Erste Praxistests auf echter Hardware (Taster, Wasserstand, WLAN-Übernahme, WebUI).

### Geändert
- „Bei wenig Wasser blinken": löst jetzt bei Pegel ≤ 20 % aus, ohne die zusätzliche Rohwert-Prüfung (`<51`), die
  auf dem Testgerät nie erfüllt war (trockene Pads lagen bei ~53–56 statt darunter)
- Standard-Hostname/mDNS-Name ist jetzt `GreenBox` + erste 4 MAC-Bytes (z. B. `GreenBox8C4B14C8`) statt
  fix `greenbox` — eindeutig pro Gerät, wie im Original-Namensschema. Bereits geflashte Geräte übernehmen den
  neuen Namen beim ersten Boot mit dieser Version

### Hinzugefügt
- Root-`README.md` mit Projektüberblick, Variantenvergleich und Hinweis, dass der Erstflash zwingend per UART
  erfolgen muss (Original-OTA akzeptiert nur das eigene Zertifikat)

### Verifiziert auf Hardware
- Erstflash per UART (esptool 5.6.0) erfolgreich, WLAN-Zugang aus Werks-Firmware korrekt übernommen
- Touch-Taster: trocken ~83, berührt ~23 (Delta ~60) — Override schaltet auto → an → aus wie im Original
- Wasserstand: trocken alle Pads ~53–56, nass ~3–5; Pegel-Stufen 0/20/50/100 bestätigt (unten/mitte/oben = A/B/C)

## [5.0.1] - 2026-10-15

### Hinzugefügt
- Default-OTA-URL zeigt auf `https://github.com/kurim/BerlinGreen-FW/releases/latest/download/firmware.bin`
- GitHub-Actions-Workflow `release.yml`: baut bei Tag `v*` und veröffentlicht `firmware.bin` + `version.txt`

- `flash.cmd` für Windows; `flash.sh`/`flash.cmd` nehmen optional ein Verzeichnis mit Release-Dateien entgegen
- Release enthält zusätzlich `bootloader.bin`, `partitions.bin`, `boot_app0.bin` für den Erstflash

## [5.0.0] - 2026-10-15

Erste Version der Custom-Firmware (Arduino-ESP32 2.0.14, PlatformIO). **Noch nicht auf Hardware getestet.**

### Hinzugefügt
- **BLE-Kompatibilität zur Original-Firmware 2.0.3 / App 4.x**: Service `0x00FF`, Characteristics FF01–FF09,
  Statuszyklus `T S s D d W 1 2 3 O f r`, Befehle `1/2/3`, `D/d`, `S/s`, `O`, `C`, `U`
- **LED-Steuerung wie im Original**: 3 Streifen (GPIO 23/22/21), LEDC 25 kHz / 11 Bit, Fade 27 pro 20 ms,
  Zeitplan Werktag/Wochenende mit Fenstern über Mitternacht, Override aus/an/auto
- **Wasserstand** über drei Touch-Pads (GPIO 14/27/13), Logik 1:1 aus der Original-Firmware,
  optionales Abdunkeln bei wenig Wasser
- **Touch-Taster** (GPIO 12 / Modul-Pin 14): Standard aktiv, einstellbare Schwelle (`button_delta`, Standard 5),
  nachlaufende Baseline, nur steigende Flanke, 1,5 s Sperre
- **WLAN**: Station-Modus mit Fallback-AP `GreenBox-Setup` inkl. Captive-DNS, mDNS, NTP
- **WebUI** (Single-Page, optional Basic-Auth `admin`): Licht, Zeitplan, Wasserstand, WLAN-Scan,
  Live-Anzeige der Touch-Sensoren, MQTT- und OTA-Einstellungen
- **MQTT** mit Availability (LWT) und retained State-JSON, Topics `<base>/set/<key>`, `<base>/cmd/{ota,toggle,reboot}`
- **OTA über HTTPS**: URL im WebUI anpassbar, Versionsprüfung über `version.txt` neben der `.bin`,
  optionaler täglicher Auto-Check, eingebettete Root-CAs (TLS-Prüfung standardmäßig an, `otaInsecure` abschaltbar)
- **Persistente Konfiguration** in NVS (Namespace `gb`), einmalige Übernahme von SSID/Passwort/Blink aus
  dem EEPROM der Original-Firmware
- Original-Partitionstabelle unverändert (nvs, otadata, app0, app1, spiffs)
- `flash.sh` für das UART-Erstflashen (NVS bleibt unberührt); Rollback über `esptool write_flash 0 berlingreen.bin`
- ESPHome-Variante `esphome/greenbox.yaml` als Alternative (nur Konfiguration validiert, nicht gebaut)

### Bekannt / Hinweise
- Erstes Flashen nur per UART/Pogo-Pins; das Original-OTA akzeptiert nur das eigene Zertifikat
- Standard-AP-Passwort `greenbox` und kein WebUI-Passwort: beides nach der Einrichtung setzen
- `ff01` liefert echte RSSI-Werte (Original: 0); bei Problemen mit der App in `netScanJson` anpassen
- Touch-Schwellen (Taster und Wasser) sind für das jeweilige Gehäuse ggf. nachzustellen
