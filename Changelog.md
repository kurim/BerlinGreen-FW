# Changelog

Alle nennenswerten Änderungen an der GreenBox-Firmware. Format orientiert sich an
[Keep a Changelog](https://keepachangelog.com/de/1.1.0/), Versionierung nach [SemVer](https://semver.org/lang/de/).

## [Unreleased]

### Hinzugefügt
- Default-OTA-URL zeigt auf `https://github.com/kurim/BerlinGreen-FW/releases/latest/download/firmware.bin`
- GitHub-Actions-Workflow `release.yml`: baut bei Tag `v*` und veröffentlicht `firmware.bin` + `version.txt`

### Geplant
- MQTT-Transport im HA-Addon/der HA-Integration `greenbox`

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
