#include "gb.h"
#include <Preferences.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <sys/time.h>
#include <time.h>

Cfg  cfg;
Live live;

static Preferences prefs;
static bool dirty = false;
static uint32_t dirtySince = 0;

String macId() {
  uint8_t m[6];
  WiFi.macAddress(m);
  char b[13];
  snprintf(b, sizeof b, "%02x%02x%02x%02x%02x%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(b);
}

// Default-Hostname wie im Original-Namensschema: "GreenBox" + erste 4 MAC-Bytes (z.B. GreenBox8C4B14C8).
static String defaultHostname() {
  uint8_t m[6];
  WiFi.macAddress(m);
  char b[9];
  snprintf(b, sizeof b, "%02X%02X%02X%02X", m[0], m[1], m[2], m[3]);
  return "GreenBox" + String(b);
}

// ---------------------------------------------------------------- persistence
static void migrateStockEeprom() {
  // Original-Firmware: Arduino-EEPROM (NVS-Namespace "eeprom"), 256 Byte.
  //   [0..]   SSID (max 32, 0-terminiert)   [99..]  Passwort   [0xFB]  0 = "blinken bei wenig Wasser"
  if (!EEPROM.begin(256)) return;
  char s[33] = {0}, p[65] = {0};
  for (int i = 0; i < 32; i++) s[i] = EEPROM.read(i);
  for (int i = 0; i < 64; i++) p[i] = EEPROM.read(99 + i);
  bool ok = s[0] != 0 && (uint8_t)s[0] != 0xFF;
  for (int i = 0; ok && s[i]; i++) if ((uint8_t)s[i] < 32 || (uint8_t)s[i] > 126) ok = false;
  if (ok) {
    cfg.ssid = s;
    bool pok = true;
    for (int i = 0; p[i]; i++) if ((uint8_t)p[i] < 32 || (uint8_t)p[i] > 126) pok = false;
    cfg.pass = pok ? String(p) : String();
  }
  cfg.blink = (EEPROM.read(0xFB) == 0);
  EEPROM.end();
}

void cfgLoad() {
  prefs.begin("gb", false);
  if (!prefs.isKey("init")) {
    migrateStockEeprom();       // erster Start: WLAN-Zugang aus Werks-Firmware uebernehmen
    prefs.putBool("init", true);
    dirty = true;
    dirtySince = millis();
  } else {
    cfg.strip[0] = prefs.getUChar("s1", 100);
    cfg.strip[1] = prefs.getUChar("s2", 100);
    cfg.strip[2] = prefs.getUChar("s3", 100);
    cfg.switchWd = prefs.getUShort("swd", cfg.switchWd);
    cfg.switchWe = prefs.getUShort("swe", cfg.switchWe);
    cfg.durWd    = prefs.getUChar("dwd", cfg.durWd);
    cfg.durWe    = prefs.getUChar("dwe", cfg.durWe);
    cfg.override_ = prefs.getUChar("ovr", 3);
    cfg.blink    = prefs.getBool("blink", false);
    cfg.button   = prefs.getBool("btn", true);
    cfg.btnDelta = prefs.getUChar("btnd", 5);
    cfg.fwByte   = prefs.getUChar("fwb", 3);
    cfg.tz       = prefs.getString("tz", cfg.tz);
    cfg.ssid     = prefs.getString("ssid", "");
    cfg.pass     = prefs.getString("pass", "");
    cfg.hostname = prefs.getString("host", cfg.hostname);
    cfg.apPass   = prefs.getString("appw", cfg.apPass);
    cfg.webPass  = prefs.getString("webpw", "");
    cfg.otaUrl   = prefs.getString("otaurl", cfg.otaUrl);
    cfg.otaAuto  = prefs.getBool("otaauto", false);
    cfg.otaInsecure = prefs.getBool("otains", false);
    cfg.otaChannel = prefs.getUChar("otach", 0);
    cfg.mqttOn   = prefs.getBool("mqon", false);
    cfg.mqttHost = prefs.getString("mqhost", "");
    cfg.mqttPort = prefs.getUShort("mqport", 1883);
    cfg.mqttTls  = prefs.getBool("mqtls", false);
    cfg.mqttUser = prefs.getString("mquser", "");
    cfg.mqttPass = prefs.getString("mqpass", "");
    cfg.mqttBase = prefs.getString("mqbase", "");
  }
  if (cfg.apPass.length() < 8) cfg.apPass = "greenbox";
  if (cfg.hostname.length() == 0 || cfg.hostname == "greenbox") {  // leer (Erststart) oder alter Fix-Default (Upgrade)
    cfg.hostname = defaultHostname();
    dirty = true; dirtySince = millis();
  }
  applyTz();
}

static void cfgSaveNow() {
  prefs.putUChar("s1", cfg.strip[0]);
  prefs.putUChar("s2", cfg.strip[1]);
  prefs.putUChar("s3", cfg.strip[2]);
  prefs.putUShort("swd", cfg.switchWd);
  prefs.putUShort("swe", cfg.switchWe);
  prefs.putUChar("dwd", cfg.durWd);
  prefs.putUChar("dwe", cfg.durWe);
  prefs.putUChar("ovr", cfg.override_);
  prefs.putBool("blink", cfg.blink);
  prefs.putBool("btn", cfg.button);
  prefs.putUChar("btnd", cfg.btnDelta);
  prefs.putUChar("fwb", cfg.fwByte);
  prefs.putString("tz", cfg.tz);
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("pass", cfg.pass);
  prefs.putString("host", cfg.hostname);
  prefs.putString("appw", cfg.apPass);
  prefs.putString("webpw", cfg.webPass);
  prefs.putString("otaurl", cfg.otaUrl);
  prefs.putBool("otaauto", cfg.otaAuto);
  prefs.putBool("otains", cfg.otaInsecure);
  prefs.putUChar("otach", cfg.otaChannel);
  prefs.putBool("mqon", cfg.mqttOn);
  prefs.putString("mqhost", cfg.mqttHost);
  prefs.putUShort("mqport", cfg.mqttPort);
  prefs.putBool("mqtls", cfg.mqttTls);
  prefs.putString("mquser", cfg.mqttUser);
  prefs.putString("mqpass", cfg.mqttPass);
  prefs.putString("mqbase", cfg.mqttBase);
}

void cfgDirty() { dirty = true; dirtySince = millis(); }

void cfgLoop() {
  if (dirty && millis() - dirtySince > 2000) {  // entprellt, schont den Flash
    cfgSaveNow();
    dirty = false;
  }
}

// ---------------------------------------------------------------- time
void applyTz() {
  // fwByte < 4: Original-Semantik, Zeitplan in UTC (App rechnet Ortszeit -> UTC).
  // fwByte >= 4: lokale Zeit, Zeitzone als POSIX-TZ (ff09 / WebUI).
  const char* tz = (cfg.fwByte >= 4 && cfg.tz.length()) ? cfg.tz.c_str() : "UTC0";
  setenv("TZ", tz, 1);
  tzset();
}

bool timeIsValid() { return time(nullptr) > 1700000000; }

void setSystemTime(uint32_t unixUtc) {
  struct timeval tv = {(time_t)unixUtc, 0};
  settimeofday(&tv, nullptr);
}

// ---------------------------------------------------------------- JSON helpers
static String jesc(const String& s) {
  String o;
  o.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) { o += ' '; }
    else o += c;
  }
  return o;
}

static String hhmm(uint16_t v) {
  char b[8];
  snprintf(b, sizeof b, "%02u:%02u", v / 100, v % 100);
  return String(b);
}

String stateJson() {
  const char* ov = cfg.override_ == 0 ? "off" : cfg.override_ == 1 ? "on" : "auto";
  String j;
  j.reserve(700);
  j += "{";
  j += "\"stripe1\":" + String(cfg.strip[0]) + ",\"stripe2\":" + String(cfg.strip[1]) + ",\"stripe3\":" + String(cfg.strip[2]);
  j += ",\"override\":\"" + String(ov) + "\"";
  j += ",\"light_on\":" + String(lightIsOn() ? "true" : "false");
  j += ",\"start\":\"" + hhmm(cfg.switchWd) + "\",\"start_weekend\":\"" + hhmm(cfg.switchWe) + "\"";
  j += ",\"duration\":" + String(cfg.durWd) + ",\"duration_weekend\":" + String(cfg.durWe);
  j += ",\"water\":" + String(live.water);
  j += ",\"low_water\":" + String(live.lowWater ? "true" : "false");
  j += ",\"blink_on_low_water\":" + String(cfg.blink ? "true" : "false");
  j += ",\"button_enabled\":" + String(cfg.button ? "true" : "false") + ",\"button_delta\":" + String(cfg.btnDelta);
  j += ",\"touch\":[" + String(live.touch[0]) + "," + String(live.touch[1]) + "," + String(live.touch[2]) + "," + String(live.touch[3]) + "]";
  j += ",\"touch_base\":" + String(live.btnBase) + ",\"button_pressed\":" + String(live.btnPressed ? "true" : "false");
  j += ",\"time_valid\":" + String(live.timeValid ? "true" : "false");
  j += ",\"time\":" + String((uint32_t)time(nullptr));
  j += ",\"fw\":\"" FW_VERSION "\",\"fw_byte\":" + String(cfg.fwByte);
  j += ",\"tz\":\"" + jesc(cfg.tz) + "\"";
  j += ",\"wifi\":{\"connected\":" + String(netConnected() ? "true" : "false") +
       ",\"ssid\":\"" + jesc(netSsid()) + "\",\"rssi\":" + String(netRssi()) +
       ",\"ip\":\"" + netIp() + "\",\"ap\":" + String(netApActive() ? "true" : "false") + "}";
  j += ",\"ota\":{\"url\":\"" + jesc(cfg.otaUrl) + "\",\"auto\":" + String(cfg.otaAuto ? "true" : "false") +
       ",\"insecure\":" + String(cfg.otaInsecure ? "true" : "false") +
       ",\"progress\":" + String(live.otaProgress) + ",\"error\":" + String(live.otaError) +
       ",\"msg\":\"" + jesc(live.otaMsg) + "\",\"latest\":\"" + jesc(live.otaLatest) + "\"}";
  j += ",\"mqtt\":{\"enabled\":" + String(cfg.mqttOn ? "true" : "false") +
       ",\"connected\":" + String(mqttConnected() ? "true" : "false") +
       ",\"host\":\"" + jesc(cfg.mqttHost) + "\",\"port\":" + String(cfg.mqttPort) +
       ",\"tls\":" + String(cfg.mqttTls ? "true" : "false") +
       ",\"user\":\"" + jesc(cfg.mqttUser) + "\",\"base\":\"" + jesc(cfg.mqttBase) + "\"}";
  j += ",\"hostname\":\"" + jesc(cfg.hostname) + "\",\"id\":\"" + macId() + "\"";
  j += ",\"uptime\":" + String(millis() / 1000) + ",\"heap\":" + String(ESP.getFreeHeap());
  j += ",\"web_password_set\":" + String(cfg.webPass.length() ? "true" : "false");
  j += "}";
  return j;
}

// ---------------------------------------------------------------- generic setter
static bool parseHhmm(const String& v, uint16_t& out) {
  int h = -1, m = -1;
  if (v.indexOf(':') > 0) {
    h = v.substring(0, v.indexOf(':')).toInt();
    m = v.substring(v.indexOf(':') + 1).toInt();
  } else {
    int n = v.toInt();
    h = n / 100; m = n % 100;
  }
  if (h < 0 || h > 23 || m < 0 || m > 59) return false;
  out = h * 100 + m;
  return true;
}

static bool parseBool(const String& v) {
  return v == "1" || v == "true" || v == "on" || v == "ON" || v == "True";
}

bool applySetting(const String& key, const String& v) {
  bool ok = true;
  if (key == "stripe1" || key == "s1" || key == "stripe2" || key == "s2" || key == "stripe3" || key == "s3") {
    int idx = key.charAt(key.length() - 1) - '1';
    cfg.strip[idx] = constrain(v.toInt(), 0, 100);
    if (cfg.override_ == 0) {}  // Helligkeit gilt, sobald das Licht an ist
  } else if (key == "override") {
    if (v == "on" || v == "1") cfg.override_ = 1;
    else if (v == "off" || v == "0") cfg.override_ = 0;
    else if (v == "auto" || v == "3") cfg.override_ = 3;
    else ok = false;
  } else if (key == "start" || key == "swd") ok = parseHhmm(v, cfg.switchWd);
  else if (key == "start_weekend" || key == "swe") ok = parseHhmm(v, cfg.switchWe);
  else if (key == "duration" || key == "dwd") cfg.durWd = constrain(v.toInt(), 0, 24);
  else if (key == "duration_weekend" || key == "dwe") cfg.durWe = constrain(v.toInt(), 0, 24);
  else if (key == "blink") cfg.blink = parseBool(v);
  else if (key == "button") cfg.button = parseBool(v);
  else if (key == "button_delta") cfg.btnDelta = constrain(v.toInt(), 1, 50);
  else if (key == "tz") { cfg.tz = v; applyTz(); }
  else if (key == "fwbyte") { cfg.fwByte = constrain(v.toInt(), 1, 9); applyTz(); }
  else if (key == "time") { setSystemTime((uint32_t)strtoul(v.c_str(), nullptr, 10)); live.timeValid = timeIsValid(); }
  else if (key == "hostname") cfg.hostname = v;
  else if (key == "ap_password") { if (v.length() >= 8) cfg.apPass = v; else ok = false; }
  else if (key == "web_password") cfg.webPass = v;
  else if (key == "ota_url") cfg.otaUrl = v;
  else if (key == "ota_auto") cfg.otaAuto = parseBool(v);
  else if (key == "ota_insecure") cfg.otaInsecure = parseBool(v);
  else if (key == "mqtt_enabled") cfg.mqttOn = parseBool(v);
  else if (key == "mqtt_host") cfg.mqttHost = v;
  else if (key == "mqtt_port") cfg.mqttPort = constrain(v.toInt(), 1, 65535);
  else if (key == "mqtt_tls") cfg.mqttTls = parseBool(v);
  else if (key == "mqtt_user") cfg.mqttUser = v;
  else if (key == "mqtt_password") cfg.mqttPass = v;
  else if (key == "mqtt_base") cfg.mqttBase = v;
  else ok = false;
  if (ok) cfgDirty();
  return ok;
}
