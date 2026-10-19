// GreenBox custom firmware - shared state and module interfaces.
// BLE-Protokoll kompatibel zur Original-Firmware 2.0.3 / App 4.x (siehe ../../PROTOCOL.md und BerlinGreen_GreenBox_RE.md).
#pragma once
#include <Arduino.h>

#define FW_VERSION "5.0.0"

struct Cfg {
  // Licht / Zeitplan (wie Original; hier zusaetzlich persistent)
  uint8_t  strip[3]   = {100, 100, 100};  // 0..100 %
  uint16_t switchWd   = 1800;             // HHMM
  uint16_t switchWe   = 1800;
  uint8_t  durWd      = 12;               // Stunden
  uint8_t  durWe      = 0;                // 0 = wie Werktag
  uint8_t  override_  = 3;                // 0 aus, 1 an, 3 auto
  bool     blink      = false;            // "bei wenig Wasser blinken"
  bool     button     = true;             // Touch-Taster GPIO12 (IO12 / Modul-Pin 14, Pad unter dem Gehaeuse)
  uint8_t  btnSamples = 3;                // Abtastungen (je 50 ms) in Folge unter der Schwelle bis zur Ausloesung
  uint8_t  btnDelta   = 5;                // Ausloesung, wenn Wert um > delta unter die Baseline faellt (Original: 5)
  // Kompatibilitaet
  uint8_t  fwByte     = 3;                // 'f' auf ff05: 3 = "2.0.3" (UTC-Semantik), >=4 = lokale Zeit + ff09-TZ
  String   tz         = "CET-1CEST,M3.5.0,M10.5.0/3";  // nur bei fwByte >= 4
  // WLAN
  String   ssid, pass;
  String   hostname;                      // leer = "GreenBox" + erste 4 MAC-Bytes, z.B. GreenBox8C4B14C8 (siehe macId())
  String   apPass     = "greenbox";       // Fallback-AP "GreenBox-Setup" (>= 8 Zeichen)
  String   webPass;                       // leer = kein Passwort (Benutzer: admin)
  // OTA
  String   otaUrl     = "https://github.com/kurim/BerlinGreen-FW/releases/latest/download/firmware.bin";
  bool     otaAuto    = false;            // taeglich auf neue Version pruefen und installieren
  bool     otaInsecure = false;           // TLS-Zertifikat nicht pruefen
  uint8_t  otaChannel = 0;                // 'C' von der App
  // MQTT
  bool     mqttOn     = false;
  String   mqttHost;
  uint16_t mqttPort   = 1883;
  bool     mqttTls    = false;
  String   mqttUser, mqttPass;
  String   mqttBase;                      // leer -> "greenbox/<mac>"
};

struct Live {
  uint8_t  water      = 255;              // 0/20/50/100, 255 = noch nicht gemessen
  bool     lowWater   = false;
  uint16_t touch[4]   = {0, 0, 0, 0};     // Rohwerte GPIO14, 27, 13, 12
  uint16_t btnBase    = 0;                // nachlaufende Baseline GPIO12
  bool     btnPressed = false;
  bool     timeValid  = false;
  int      otaProgress = 0;               // 0..100
  int      otaError   = 0;
  String   otaMsg;
  String   otaLatest;                     // zuletzt gefundene Version
};

extern Cfg  cfg;
extern Live live;

// config.cpp
void   cfgLoad();
void   cfgDirty();            // spaeter speichern (entprellt)
void   cfgLoop();
String macId();               // 12 hex, ohne ':'
String stateJson();           // fuer WebUI + MQTT
void   applyTz();             // TZ-Env gemaess fwByte/tz
bool   timeIsValid();
void   setSystemTime(uint32_t unixUtc);

// light.cpp
void lightBegin();
void lightLoop();
bool lightIsOn();
void lightToggleOverride();   // Taster / MQTT

// ble.cpp
void bleBegin();
void bleLoop();
void bleStatusChanged();      // ff03 + ff06 benachrichtigen
bool bleClients();

// net.cpp
void   netBegin();
void   netLoop();
bool   netConnected();
bool   netApActive();
String netStatusJson();       // ff03-Format: ["c","SSID",rssi] | ["d"]
String netScanJson(bool app);// app=true: [["SSID",rssi],...] (ff01-Format), false: Objekte fuer WebUI
void   netRequestScan();
void   netRequestConnect(const String& ssid, const String& pass);
void   netRequestOta();
void   netRequestCheckUpdate();
void   netReboot();
String netIp();
int    netRssi();
String netSsid();

// mqtt.cpp
void mqttBegin();
void mqttLoop();
void mqttPublishState();
bool mqttConnected();
// Setter, gemeinsam fuer BLE, Web und MQTT (key-basiert, liefert false bei unbekanntem Key/Wert)
bool applySetting(const String& key, const String& value);

// web.cpp
void webBegin();
void webLoop();
