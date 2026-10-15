# BerlinGreen „GreenBox" — Reverse-Engineering & Umbau-Doku

> Ziel: WLAN nicht nur für FOTA, sondern vollständig nutzbar machen; optional ein rudimentäres WebUI.
> Basis: Flash-Dump `berlingreen_1.bin` (8 MB), ESP32-WROOM-32E, unverschlüsselt & ohne Secure Boot.

---

## 0. Status & TL;DR

- Der Chip ist **komplett offen** (keine Flash Encryption, kein Secure Boot, UART-Download aktiv, JTAG aktiv). Du kannst beliebig auslesen, eigene Firmware flashen und jederzeit den Werkszustand zurückspielen.
- Die Stockfirmware **kann bereits WLAN** (STA verbindet sich, macht HTTPS-OTA), nutzt es aber praktisch nur dafür. Die Steuerung läuft über **BLE** (`GreenBox`-GATT-Service).
- Für dein Ziel (WLAN voll nutzbar + WebUI auf dem Gerät) führt kein Weg an **eigener Firmware** vorbei — ein Webserver lässt sich nicht in das kompilierte Binary nachrüsten. Die gute Nachricht: die App-Partition ist **3,7 MB** groß, Platz für ein WebUI ist also reichlich da.
- **Einziger fehlender Baustein:** die GPIO-Belegung (welcher Pin treibt welchen LED-Kanal / liest Wasserstand / Temperatur). Die steht nicht im Dump und muss per Durchgangsprüfung + Boot-Log ermittelt werden. Siehe Abschnitt 6.

---

## 1. Hardware

| | |
|---|---|
| Modul | Espressif **ESP32-WROOM-32E** (Xtensa LX6 Dual-Core, Rev. v3) |
| Flash | **8 MB** (Flash-Header-Nibble `3` @ `0x1003`; Dump ist 8 MB) |
| Stromversorgung | externes **12 V / 3 A** Netzteil |
| 3V3-Erzeugung | Buck-Wandler (Spule „8R2" = 8,2 µH) auf der Platine |
| Eingangssicherung | 5 A im 12-V-Pfad |
| LED-Last | 3 Streifen auf 12 V, über MOSFETs geschaltet (PWM) |
| Debug-Zugang | **Tag-Connect TC2030** Footprint unter dem Modul (6 Pads, 3 Führungslöcher) |
| USB | Micro-USB nur Versorgung/Laden (Nachbarchip „DE1" = ESD-Array, **kein** USB-UART) |

### eFuse-Sicherheitsstatus (aus `espefuse summary`)

| Fuse | Wert | Bedeutung |
|---|---|---|
| `FLASH_CRYPT_CNT` | 0 | **unverschlüsselt** |
| `ABS_DONE_0/1` | False | **kein Secure Boot** |
| `UART_DOWNLOAD_DIS` | False | UART-Download offen |
| `JTAG_DISABLE` | False | JTAG offen (Live-Debug möglich) |
| BLOCK1–3 | alle 0 | keine Keys gebrannt |
| MAC | `8c:4b:14:c8:3d:6c` | — |

> ⚠️ Alle eFuses stehen auf R/W. Beim Arbeiten **nie** versehentlich `espefuse burn-*` absetzen — das ist irreversibel. Auslesen und Flashen ändern nichts an den eFuses.

---

## 2. Flash-Layout (Partitionstabelle @ `0x8000`)

| Name | Typ | Offset | Größe | Inhalt |
|---|---|---|---|---|
| `nvs` | data/nvs | `0x009000` | 20 K | WLAN-Config, PHY-Kalibrierung, EEPROM-Emulation |
| `otadata` | data/ota | `0x00e000` | 8 K | zeigt auf aktives App-Slot |
| `app0` | app/ota_0 | `0x010000` | 3712 K | **ältere** Firmware (Fallback) |
| `app1` | app/ota_1 | `0x3b0000` | 3712 K | **aktive** Firmware (OTA-seq = 2) |
| `spiffs` | data/spiffs | `0x750000` | 192 K | **leer** (kein WebUI/Assets) |
| *(frei)* | — | `0x780000` | 512 K | unbenutzt (nur `0xFF`) |

**Aktiv ist `app1`.** Die beiden Images unterscheiden sich leicht: app1 ist neuer (enthält `BlinkOnLowWater`, `waterNotifyIfChanged`, Licht-Zeitplan-Logik), app0 ist das ältere Werks-Image.

---

## 3. Firmware-Framework

- **Arduino-ESP32 Core 1.0.6** (`esp32-arduino-lib-builder`, IDF `v3.3.5-1-g85c43024c`, Build `Mar 26 2021`).
- Gebaut mit **PlatformIO** (Build-Pfade `C:/Users/MichaelMcIntyre/.platformio/...`).
- Entwickelt von **Voxdale** (Log-String `VoxdaleTest,%lu,%d,%d,%d,%d,%d`).
- Verwendete Libs laut Strings: `BLE`, `WiFi`, `EEPROM`, `Update/HttpsOTAUpdate`, ein MQTT-Client (PubSubClient-typisch: `SUBSCRIBE`/`UNSUBSCRIBE`, „Message arrived on topic").

---

## 4. BLE-Protokoll (`GreenBox`)

Advertising-Name: **`GreenBox`**. Ein GATT-Service mit sechs Characteristics `0xFF01`–`0xFF06` (Basis-UUID `0000FFxx-0000-1000-8000-00805f9b34fb`), plus Service-UUID-Kennung `0x00FF`.

| UUID | vermutete Funktion (aus Log-Strings abgeleitet) |
|---|---|
| `0xFF01` | WiFi-AP-Liste lesen (`Get Wifi AP LIst Chara`, `Send WifiAPCharacteristics Notify`) |
| `0xFF02` | WiFi-Zugangsdaten setzen (`Set Wifi AP LIst Chara`) |
| `0xFF03` | WiFi-Status / `Send GetWifiCharacteristics Notify` |
| `0xFF04` | Kommando-Characteristic (`Write on SimpleBLE Characteristic`) |
| `0xFF05` | Wasserstand-Notify (`waterNotifyIfChanged`) |
| `0xFF06` | weitere Statuswerte |

> ⚠️ **Korrektur:** Die obige Zuordnung (insb. `0xFF04` als Kommando-Characteristic) war eine Vermutung und ist **falsch**.
> Aus dem Decompile (Vtables der Characteristic-Callbacks) und `PROTOCOL.md`: `FF01` WLAN-Scan (`gimme` schreiben, lesen),
> `FF02` WLAN-Zugang `SSID,Passwort`, `FF03` WLAN-Status (`["c","SSID",rssi]` / `["d"]`), **`FF05` Kommandos (write) + Statusstrom (notify)**,
> `FF06` Config `blink,lowWater,plus`. `FF04` existiert nicht; `FF07–FF09` nur in V4-Firmware. Details: `PROTOCOL.md`.

**Paketformat:** Beim Schreiben werden **Länge, Struktur und Checksumme** geprüft
(`received wrong packet`, `received wrong checksum`). Es gibt also ein kleines Binärprotokoll mit Prüfsumme — das genaue Format muss gesniffed werden.

### Paketformat (aus Decompile von `FUN_400d2324`, verifiziert im Code)

Alle Schreibzugriffe auf die Kommando-Characteristic sind **6 oder 8 Byte** lang, Start `0xEE`, Ende `0xEF`.
**Prüfsumme** = `(-(Summe aller Bytes des Pakets, mit Prüfsummenbyte = 0)) & 0xFF` (Zweierkomplement; `FUN_401f05cc`).

```
6 Byte:  EE  cmd  d1  d2  CK  EF           CK = -(EE+cmd+d1+d2+EF) & 0xFF
8 Byte:  EE  cmd  b2  b3  b4  b5  CK  EF   nur cmd 't' (Zeit)
```

| cmd | Bedeutung | Wert |
|---|---|---|
| `'1'` `'2'` `'3'` (0x31–33) | Helligkeit Stripe 1/2/3 | `d2` = 0…100 (%) |
| `'D'` | Leuchtdauer Werktag | `d2` in Stunden |
| `'d'` | Leuchtdauer Wochenende | `d2` in Stunden (0 = Wochenende wie Werktag) |
| `'S'` | Einschaltzeit Werktag | `d1*256+d2`, **HHMM** dezimal (z. B. 1830 = 18:30) |
| `'s'` | Einschaltzeit Wochenende | wie `'S'` |
| `'O'` | Override | `d2`: 0 = AUS, 1 = EIN, 3 = AUTO (Zeitplan) |
| `'t'` | Uhrzeit setzen (8 Byte) | `b2..b5` = Unix-Zeit, **big-endian** |

Beispiel „Stripe 1 = 50 %": `EE 31 00 32 C0 EF` mit `CK = -(0xEE+0x31+0x00+0x32+0xEF) & 0xFF = 0xC0`.

> **WLAN und BLE schließen sich aus:** Ein Flag in der EEPROM-Emulation (Byte 250) entscheidet beim Boot. Mit gesetztem WLAN-Modus verbindet die Firmware per STA, prüft periodisch auf OTA und startet **kein** BLE. Ohne WLAN-Modus läuft nur BLE. Das erklärt, warum WLAN praktisch nur für FOTA genutzt wird.

### Datenmodell (Config-Felder, die über BLE ausgetauscht werden)

```
Stripe1 / Stripe2 / Stripe3     drei LED-Kanäle
duration / durationWeeekend     Leuchtdauer Werktag / Wochenende  [sic: Tippfehler im Original]
switchtime / switchtimeWeekend  Einschaltzeit Werktag / Wochenende
onOffOverride                   manueller Override (ON/OFF/AUTO)
timeofday                       Uhrzeit-Sync
```

Weitere Zustände/Effekte aus den Strings: `Light should be ON/OFF`, `Light overriddewn to ON/OFF` [sic], `Light shedule skipped since time is not set` [sic], `Breathe` (Atem-Effekt), `set to 100%`, `BlinkOnLowWater`, `The temperature is %f degree C`.

---

## 5. OTA & Cloud

- Update-URL fest einkompiliert: `https://upotaspre.berlingreen.com/firmware.bin`
- Eingebettetes **self-signed Root-Zertifikat** (CN `updateesp.berlingreen.com`, O „Berlin Green", gültig bis 2032). Firmware wird nur gegen dieses eine Zertifikat akzeptiert.
- MQTT-Client ist vorhanden, aber **kein Broker-Host und kein Topic** fest hinterlegt → Cloud läuft offenbar nur über BLE-Setup + HTTPS-OTA, keine dauerhafte MQTT-Verbindung aktiv.

---

## 6. NVS-Inhalt

Die `nvs`-Partition enthält (per `nvs.py`, s. u.):

- `nvs.net80211`: hinterlegtes STA-Profil (SSID + Passwort **im Klartext**), AP-Config, letzte Kanal-/BSSID-Infos. → WLAN-Zugang ist also eingerichtet und funktioniert.
- `phy`: RF-Kalibrierungsdaten (`cal_data`, `cal_mac`).
- `eeprom`-Namespace: 256-Byte EEPROM-Emulation. Enthält im Dump v. a. die SSID als String; die Zeitplan-Config liegt **nicht** hier (wird im RAM/zur Laufzeit gehalten bzw. per BLE gesetzt).

> 🔒 In der NVS stehen deine **WLAN-Zugangsdaten im Klartext** (normal bei diesen Geräten). Den Dump vertraulich behandeln, nicht öffentlich anhängen. Die Werte kannst du dir lokal mit `nvs.py` ansehen.

---

## 7. Lokale Re-Analyse — Befehle

### 7.1 Dump & eFuses (zur Erinnerung, schon erledigt)
```bash
esptool --port COMx flash-id
espefuse --port COMx summary
esptool --port COMx --baud 460800 read-flash 0 ALL berlingreen_1.bin
# zur Sicherheit zweimal und Hashes vergleichen:
certutil -hashfile berlingreen_1.bin SHA256
```

### 7.2 Partitionen herausschneiden
```bash
# Partitionstabelle dekodieren
python esp-idf/components/partition_table/gen_esp32part.py <(dd if=berlingreen_1.bin bs=1 skip=$((0x8000)) count=3072)

# aktives App-Image (app1)
dd if=berlingreen_1.bin of=app1.bin bs=1 skip=$((0x3b0000)) count=$((0x181e20))
esptool --chip esp32 image-info app1.bin
```

### 7.3 Strings & UUIDs
```bash
strings -n 6 app1.bin > app1_strings.txt
grep -oiE '[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}' app1_strings.txt | sort -u
```

### 7.4 NVS parsen
Das beiliegende `nvs.py` (reiner Python-Parser, keine IDF nötig) liest Namespaces, Key/Value-Paare und Blobs direkt aus dem Dump:
```bash
python3 nvs.py berlingreen_1.bin
```

### 7.5 Disassembly (für das BLE-Paketformat & GPIO-Config)
Ghidra ab 11.x bringt **Xtensa** nativ mit. Ablauf:
1. `esp32_image_parser` (Tenable) → aus `app1.bin` ein ELF bauen:
   ```bash
   python esp32_image_parser.py create_elf app1.bin -o app1.elf -r
   ```
2. ELF in Ghidra importieren (Sprache: `Xtensa:LE:32:default`). Alternativ das `ghidra-esp32-flash-loader`-Plugin, das die Segmente aus dem Image direkt mappt.
3. Einstieg über die bekannten Strings: in Ghidra nach `Stripe1:`, `switchtime:`, `Write on SimpleBLE Characteristic` suchen → Xrefs → dort stehen das Parsing des BLE-Pakets und die `ledcWrite`/`analogWrite`/`digitalWrite`-Aufrufe mit den **GPIO-Nummern als Konstanten**.

---

## 8. GPIO-Belegung ermitteln (der fehlende Baustein)

Zwei sich ergänzende Wege:

**A) Hardware-seitig (Durchgangsprüfung, am verlässlichsten):**
- Die drei **MOSFET-Gates** zurück zum Modul messen → das sind `Stripe1/2/3` (PWM).
- Wasserstandssensor-Anschluss → Modulpin (vermutlich ein ADC-fähiger GPIO: 32–39).
- Temperaturquelle lokalisieren (NTC/DS18B20? oder interner Sensor — der String `The temperature is … degree C` sagt es nicht).
- Referenz-Pins am Modul: GND=1, 3V3=2, EN=3, IO0=25, RXD0=34, TXD0=35.

**B) Firmware-seitig (Ghidra):** die GPIO-Nummern stehen als Konstanten in den `ledc_*`/`gpio_set_*`-Aufrufen nahe den obigen Strings. A und B gegeneinander prüfen.

Trage die Ergebnisse hier ein:

**Ergebnis der Firmware-Analyse (Ghidra, `app1` = OTA `firmware.bin`, byte-identisch):**

| Funktion | GPIO | PWM-Kanal | Notiz |
|---|---|---|---|
| Stripe1 | **23** | LEDC ch0 | `ledcAttachPin(23,0)`, 25 kHz, 11 Bit |
| Stripe2 | **22** | LEDC ch1 | `ledcAttachPin(22,1)` |
| Stripe3 | **21** | LEDC ch2 | `ledcAttachPin(21,2)` |
| Wasserstand A | **14** | – | `touchRead` (T6), Median aus 11 Samples |
| Wasserstand B | **27** | – | `touchRead` (T7) |
| Wasserstand C | **13** | – | `touchRead` (T4) |
| Taster | **12** | – | **Touch-Pad (T5)**, `touchAttachInterrupt(12, isr, baseline-5)` → Override umschalten |
| Temperatur | – | | Der String `The temperature is…` hat **keine Referenz** (toter Code); kein Sensor-Zugriff gefunden |
| Status-LED | – | | keine gefunden |

> Stripes: 12-V-MOSFET-Gates → Duty 0…100 % wird auf `value*1340/100 (+100 Offset)` bei max. 11 Bit (0–2047) skaliert; Fade in Schritten von 27 mit 10 ms Delay.
> Wasserstand: drei **kapazitive Touch-Pads** (niedriger Wert = benetzt). Ableitung (aus Decompile, Schwelle 10): A<10 ∧ B<10 ∧ C<10 → 100 %; A<10 ∧ B<10 ∧ C≥10 → 50 %; A<10 ∧ B≥10 ∧ C≥10 → 20 %; sonst 0 %. Low-Water-Blink bei Level ≤20 % und alle Werte <51, wenn `BlinkOnLowWater` (EEPROM) aktiv ist.
> Hardware-Gegenprobe (Durchgang) weiterhin empfohlen, v. a. Touch-Pads (Wasserstand) und Taster.

---

## 9. Umbau-Plan: volles WLAN + WebUI

Da ein Webserver nicht nachrüstbar ins Stock-Binary passt, ist das Ziel = **eigene Firmware**. Empfohlene Reihenfolge:

1. **Werks-Backup sichern** (hast du: `berlingreen_1.bin`). Rückweg jederzeit:
   ```bash
   esptool --port COMx write-flash 0 berlingreen_1.bin
   ```
2. **GPIOs ermitteln** (Abschnitt 8) — ohne die geht nichts.
3. **Entscheidung Firmware-Basis:**
   - **ESPHome** — schnellster Weg zu WLAN + HA-Integration. Für das WebUI: `web_server:` Komponente (reicht für „rudimentär"). LED-Kanäle als `monochromatic`/`ledc`-Lights, Wasserstand als `adc`/`binary_sensor`.
   - **Arduino + ESPAsyncWebServer** — maximale Freiheit für ein eigenes WebUI, mehr Arbeit.
4. **Partition-Scheme:** bei 8 MB bequem `min_spiffs`/`default_8MB` mit großer App + LittleFS fürs WebUI. Platz ist unkritisch.
5. **Flashen & testen**, GPIO-Zuordnung verifizieren (erst mit niedriger Helligkeit, 12-V-Streifen können).

### ESPHome-Skelett (Platzhalter-GPIOs anpassen!)
```yaml
esphome:
  name: greenbox
esp32:
  board: esp32dev
  framework:
    type: arduino

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  ap:                      # Fallback-AP, falls WLAN weg ist
    ssid: "GreenBox-Setup"

captive_portal:
web_server:                # rudimentäres WebUI
  port: 80
api:                       # Home-Assistant-Anbindung
ota:

output:
  - platform: ledc
    pin: GPIOxx            # Stripe1  <-- aus Abschnitt 8
    id: out_stripe1
  - platform: ledc
    pin: GPIOxx            # Stripe2
    id: out_stripe2
  - platform: ledc
    pin: GPIOxx            # Stripe3
    id: out_stripe3

light:
  - platform: monochromatic
    name: "GreenBox Stripe 1"
    output: out_stripe1
  - platform: monochromatic
    name: "GreenBox Stripe 2"
    output: out_stripe2
  - platform: monochromatic
    name: "GreenBox Stripe 3"
    output: out_stripe3

sensor:
  - platform: adc
    pin: GPIOxx            # Wasserstand  <-- prüfen ob ADC
    name: "GreenBox Wasserstand"
    attenuation: 11db
```

### Alternative (nicht-invasiv, kein Umbau)
Falls du die Stockfirmware behalten willst: BLE sniffen und `GreenBox` von HA aus über `bleak`/ESPHome-`ble_client` ansteuern. Gibt aber **kein** volles WLAN/WebUI auf dem Gerät — passt daher nicht zu deinem Ziel, nur als Rückfalloption notiert.

---

## 10. Offene Punkte / To-do

- [ ] GPIO-Tabelle (Abschnitt 8) ausfüllen
- [ ] BLE-Paketformat (`0xFF04`) per Sniff dekodieren — optional, nur falls Stock-Kompatibilität gewünscht
- [ ] Temperatur-Sensortyp klären
- [ ] Partition-Scheme für Custom-Firmware festlegen
- [ ] ESPHome-Config mit echten GPIOs testen
- [ ] Werks-Backup an sicherem Ort ablegen (mit SHA256 notiert)

### Verifizierte Fakten (SHA256 des Dumps)
```
berlingreen_1.bin  SHA256: fc0740afea16b430aa64d5dd38056194828a398894380b9f524229d350a41e37
```
