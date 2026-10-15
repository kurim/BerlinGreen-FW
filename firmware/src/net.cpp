// WLAN (STA + Fallback-AP), NTP, mDNS, WLAN-Scan/-Einrichtung (BLE ff01-ff03), OTA mit konfigurierbarer URL.
#include "gb.h"
#include "cacerts.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <esp_wifi.h>

static const char* AP_SSID = "GreenBox-Setup";

struct ApEntry { String ssid; int rssi; };
static ApEntry scanRes[20];
static int scanCount = 0;
static bool scanReq = false, scanDone = false;

static bool connReq = false, connBusy = false;
static String pendSsid, pendPass;
static uint32_t connDeadline = 0;

static bool apUp = false;
static uint32_t apStart = 0, staStart = 0, lastRetry = 0;
static DNSServer dns;
static bool wasConnected = false;
static bool mdnsUp = false;

static bool otaReq = false, checkReq = false;
static uint32_t lastAutoCheck = 0;
static bool rebootReq = false;
static uint32_t rebootAt = 0;

bool   netConnected() { return WiFi.status() == WL_CONNECTED; }
bool   netApActive()  { return apUp; }
String netIp()        { return netConnected() ? WiFi.localIP().toString() : (apUp ? WiFi.softAPIP().toString() : String("")); }
int    netRssi()      { return netConnected() ? WiFi.RSSI() : 0; }
String netSsid()      { return netConnected() ? WiFi.SSID() : String(""); }
void   netReboot()    { rebootReq = true; rebootAt = millis() + 800; }
void   netRequestScan() { scanReq = true; }
void   netRequestOta()  { otaReq = true; }
void   netRequestCheckUpdate() { checkReq = true; }

static String jq(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') o += '\\';
    if ((uint8_t)c >= 0x20) o += c;
  }
  return o;
}

String netStatusJson() {            // ff03: ["c","SSID",rssi] | ["d"]
  if (!netConnected()) return "[\"d\"]";
  return "[\"c\",\"" + jq(WiFi.SSID()) + "\"," + String(WiFi.RSSI()) + "]";
}

String netScanJson(bool app) {
  String j = "[";
  for (int i = 0; i < scanCount; i++) {
    if (i) j += ",";
    if (app) j += "[\"" + jq(scanRes[i].ssid) + "\"," + String(scanRes[i].rssi) + "]";
    else     j += "{\"ssid\":\"" + jq(scanRes[i].ssid) + "\",\"rssi\":" + String(scanRes[i].rssi) + "}";
    if (app && j.length() > 470) break;   // GATT-Attribut max. 512 Byte
  }
  j += "]";
  return j;
}

void netRequestConnect(const String& ssid, const String& pass) {
  pendSsid = ssid;
  pendPass = pass;
  connReq = true;
}

// ---------------------------------------------------------------- internals
static void doScan() {
  int n = WiFi.scanNetworks(false, false);
  scanCount = 0;
  for (int i = 0; i < n && scanCount < 20; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    bool dup = false;
    for (int k = 0; k < scanCount; k++) if (scanRes[k].ssid == s) dup = true;
    if (dup) continue;
    scanRes[scanCount++] = {s, WiFi.RSSI(i)};
  }
  WiFi.scanDelete();
  scanDone = true;
}

static void startAp() {
  if (apUp) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, cfg.apPass.c_str());
  dns.start(53, "*", WiFi.softAPIP());   // Captive Portal
  apUp = true;
  apStart = millis();
  Serial.printf("AP up: %s  %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
}

static void stopAp() {
  if (!apUp) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apUp = false;
}

static void startMdns() {
  if (mdnsUp) MDNS.end();
  mdnsUp = MDNS.begin(cfg.hostname.c_str());
  if (mdnsUp) MDNS.addService("http", "tcp", 80);
}

// ---------------------------------------------------------------- OTA
static String versionUrl() {
  int i = cfg.otaUrl.lastIndexOf('/');
  return i > 0 ? cfg.otaUrl.substring(0, i + 1) + "version.txt" : String();
}

static void otaClientSetup(WiFiClientSecure& c) {
  if (cfg.otaInsecure) c.setInsecure();
  else c.setCACert(ROOT_CAS);
}

static void checkUpdate(bool install) {
  String vu = versionUrl();
  if (!vu.length() || !netConnected()) return;
  WiFiClientSecure c;
  otaClientSetup(c);
  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
  if (!http.begin(c, vu)) { live.otaMsg = "bad version url"; return; }
  int code = http.GET();
  if (code == 200) {
    String v = http.getString();
    v.trim();
    if (v.length() > 0 && v.length() < 24) {
      live.otaLatest = v;
      live.otaMsg = (v == FW_VERSION) ? "up to date" : "update available: " + v;
      if (install && v != FW_VERSION) otaReq = true;
    }
  } else {
    live.otaMsg = "version check failed (" + String(code) + ")";
  }
  http.end();
}

static void runOta() {
  if (!netConnected()) { live.otaMsg = "no wifi"; live.otaError = 1; return; }
  live.otaProgress = 0; live.otaError = 0; live.otaMsg = "downloading";
  WiFiClientSecure c;
  otaClientSetup(c);
  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  httpUpdate.rebootOnUpdate(false);
  httpUpdate.onProgress([](int cur, int total) {
    if (total > 0) live.otaProgress = (int)((int64_t)cur * 100 / total);
  });
  t_httpUpdate_return r = httpUpdate.update(c, cfg.otaUrl);
  if (r == HTTP_UPDATE_OK) {
    live.otaProgress = 100;
    live.otaMsg = "ok, rebooting";
    netReboot();
  } else {
    live.otaError = httpUpdate.getLastError() ? httpUpdate.getLastError() : 2;
    live.otaMsg = httpUpdate.getLastErrorString();
  }
}

// ---------------------------------------------------------------- lifecycle
void netBegin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(cfg.hostname.c_str());
  WiFi.setAutoReconnect(true);
  if (cfg.ssid.length()) {
    WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
    staStart = millis();
  } else {
    startAp();                                  // keine Zugangsdaten -> sofort Einrichtungs-AP
  }
}

void netLoop() {
  uint32_t now = millis();
  if (apUp) dns.processNextRequest();

  if (rebootReq && now > rebootAt) ESP.restart();

  // Verbindungsaenderung -> NTP / mDNS / Status
  bool c = netConnected();
  if (c && !wasConnected) {
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");   // Offset 0: System bleibt UTC, TZ-Env regelt Ortszeit
    startMdns();
    bleStatusChanged();
    lastAutoCheck = now - 24UL * 3600 * 1000 + 60000;           // erste Pruefung ~60 s nach Verbindung
  } else if (!c && wasConnected) {
    bleStatusChanged();
  }
  wasConnected = c;

  // BLE: WLAN-Scan ("gimme")
  if (scanReq) { scanReq = false; doScan(); }

  // BLE/Web: neue WLAN-Zugangsdaten ausprobieren (Original: 30 s, dann speichern)
  if (connReq) {
    connReq = false;
    connBusy = true;
    WiFi.disconnect(false);
    WiFi.begin(pendSsid.c_str(), pendPass.c_str());
    connDeadline = now + 30000;
  }
  if (connBusy) {
    if (c && WiFi.SSID() == pendSsid) {
      cfg.ssid = pendSsid; cfg.pass = pendPass; cfgDirty();
      connBusy = false;
      staStart = now;
      bleStatusChanged();
      Serial.println("WiFi: connected, credentials stored");
    } else if (now > connDeadline) {
      connBusy = false;
      Serial.println("WiFi: connect failed");
      if (cfg.ssid.length()) WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());   // alte Zugangsdaten wiederherstellen
      bleStatusChanged();
    }
  }

  // Fallback-AP: nach 30 s ohne WLAN; wieder aus, sobald verbunden (nach 60 s Karenz)
  if (!c && !connBusy && !apUp && cfg.ssid.length() && now - staStart > 30000) startAp();
  if (apUp && c && now - apStart > 60000) stopAp();
  if (apUp && !c && cfg.ssid.length() && now - lastRetry > 60000) {   // regelmaessig erneut versuchen
    lastRetry = now;
    WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  }

  // OTA
  if (checkReq) { checkReq = false; checkUpdate(false); }
  if (cfg.otaAuto && c && now - lastAutoCheck > 24UL * 3600 * 1000) { lastAutoCheck = now; checkUpdate(true); }
  if (otaReq) { otaReq = false; runOta(); }
}
