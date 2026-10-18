// MQTT-Schnittstelle (Topic-Schema siehe ../README.md).
//   <base>/availability   online|offline (retained, LWT)
//   <base>/state          JSON (retained)
//   <base>/set/<key>      Payload = Wert; Keys wie applySetting()
//   <base>/cmd/ota        "start" | "check"      <base>/cmd/reboot   "1"
#include "gb.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

static WiFiClient plain;
static WiFiClientSecure secure;
static PubSubClient mq;
static uint32_t lastTry = 0, lastPub = 0;
static String lastStateSent;
static uint16_t lastBtnSent = 0;
static String base;

static String baseTopic() {
  return cfg.mqttBase.length() ? cfg.mqttBase : "greenbox/" + macId();
}

bool mqttConnected() { return mq.connected(); }

static void onMessage(char* topic, uint8_t* payload, unsigned int len) {
  String t(topic), v;
  for (unsigned int i = 0; i < len; i++) v += (char)payload[i];
  v.trim();
  String pre = base + "/set/";
  if (t.startsWith(pre)) {
    String key = t.substring(pre.length());
    if (applySetting(key, v)) mqttPublishState();
    return;
  }
  if (t == base + "/cmd/ota") {
    if (v == "check") netRequestCheckUpdate();
    else netRequestOta();
  } else if (t == base + "/cmd/reboot") {
    netReboot();
  } else if (t == base + "/cmd/toggle") {
    lightToggleOverride();
    mqttPublishState();
  }
}

void mqttBegin() {
  mq.setBufferSize(1024);
  mq.setCallback(onMessage);
}

void mqttPublishState() {
  if (!mq.connected()) return;
  String s = stateJson();
  mq.publish((base + "/state").c_str(), s.c_str(), true);
  lastStateSent = s;
  lastBtnSent = live.touch[3];
  lastPub = millis();
}

void mqttLoop() {
  if (!cfg.mqttOn || !cfg.mqttHost.length() || !netConnected()) {
    if (mq.connected()) mq.disconnect();
    return;
  }
  uint32_t now = millis();
  if (!mq.connected()) {
    if (now - lastTry < 5000) return;
    lastTry = now;
    base = baseTopic();
    if (cfg.mqttTls) { secure.setInsecure(); mq.setClient(secure); }   // TLS ohne Zertifikatspruefung (LAN-Broker mit eigenem CA)
    else mq.setClient(plain);
    mq.setServer(cfg.mqttHost.c_str(), cfg.mqttPort);
    String cid = "greenbox-" + macId();
    String lwt = base + "/availability";
    bool ok = cfg.mqttUser.length()
      ? mq.connect(cid.c_str(), cfg.mqttUser.c_str(), cfg.mqttPass.c_str(), lwt.c_str(), 0, true, "offline")
      : mq.connect(cid.c_str(), nullptr, nullptr, lwt.c_str(), 0, true, "offline");
    if (!ok) return;
    mq.publish(lwt.c_str(), "online", true);
    mq.subscribe((base + "/set/#").c_str());
    mq.subscribe((base + "/cmd/#").c_str());
    mqttPublishState();
    return;
  }
  mq.loop();
  // Aenderungen sofort, sonst alle 60 s
  if (now - lastPub > 1000) {
    String s = stateJson();
    // uptime/heap/time aendern sich staendig -> nur senden wenn sich etwas Relevantes aendert oder 60 s um sind
    auto strip = [](String x) {
      int i = x.indexOf("\"time\":");
      int j = x.indexOf(",\"fw\"");
      if (i >= 0 && j > i) x.remove(i, j - i);
      i = x.indexOf(",\"uptime\"");
      j = x.indexOf(",\"web_password_set\"");
      if (i >= 0 && j > i) x.remove(i, j - i);
      // nur das Roh-Array der Touch-Werte ausblenden; touch_base und button_pressed bleiben im Vergleich,
      // damit ein Tastendruck sofort gemeldet wird
      i = x.indexOf("\"touch\":[");
      j = x.indexOf(']', i);
      if (i >= 0 && j > i) x.remove(i, j - i + 1);
      return x;
    };
    // Taster-Rohwert (IO12) fuer den HA-Verlauf: bei Aenderung ab 3 Einheiten melden (hoechstens 1x/s durch die Schleife)
    bool btnMoved = abs((int)live.touch[3] - (int)lastBtnSent) >= 3;
    if (btnMoved || strip(s) != strip(lastStateSent) || now - lastPub > 60000) mqttPublishState();
  }
}
