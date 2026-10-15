// BLE-GATT, kompatibel zur Original-Firmware 2.0.3 (App 4.x / HA-Integration).
// Service 0x00FF, Characteristics FF01..FF03, FF05, FF06 wie Original; FF07..FF09 zusaetzlich (V4-Variante der App).
#include "gb.h"
#include <NimBLEDevice.h>
#include <WiFi.h>

static NimBLEServer* server;
static NimBLECharacteristic *chAp, *chSet, *chStat, *chCmd, *chCfg, *chOta, *chInfo, *chTz;
static volatile bool notifyStatus = false;   // ff03 5x benachrichtigen (wie Original nach Verbindungsversuch)
static uint8_t statusRepeats = 0;
static uint32_t lastStatusNotify = 0, lastCycle = 0;
static uint8_t cycleIdx = 0;

// ---------------------------------------------------------------- framing
static uint8_t cksum(const uint8_t* b, size_t n) {
  uint8_t s = 0;
  for (size_t i = 0; i < n; i++) s += b[i];
  return (uint8_t)(0 - s);
}

static void notifyCmd(uint8_t type, uint16_t value) {
  uint8_t p[6] = {0xEE, type, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF), 0, 0xEF};
  p[4] = cksum(p, 6);                       // Pruefsumme: Summe aller Bytes inkl. 0xEE/0xEF == 0 (mod 256)
  chCmd->setValue(p, 6);
  chCmd->notify();
}

// ff05 Status-Zyklus exakt wie Original (FUN_400d2218): T S s D d W 1 2 3 O f r, ein Paket je 100 ms
static void cycleStep() {
  switch (cycleIdx) {
    case 0:  notifyCmd('T', 255); break;                    // Temperatur: kein Sensor
    case 1:  notifyCmd('S', cfg.switchWd); break;
    case 2:  notifyCmd('s', cfg.switchWe); break;
    case 3:  notifyCmd('D', cfg.durWd); break;
    case 4:  notifyCmd('d', cfg.durWe); break;
    case 5:  notifyCmd('W', live.water); break;
    case 6:  notifyCmd('1', cfg.strip[0]); break;
    case 7:  notifyCmd('2', cfg.strip[1]); break;
    case 8:  notifyCmd('3', cfg.strip[2]); break;
    case 9:  notifyCmd('O', cfg.override_); break;
    case 10: notifyCmd('f', cfg.fwByte); break;
    case 11: notifyCmd('r', 1); break;                      // Hardware-Revision
  }
  cycleIdx = (cycleIdx + 1) % 12;
}

// ---------------------------------------------------------------- incoming packets (ff05)
static void handlePacket(const uint8_t* p, size_t n) {
  if (n == 6 && p[0] == 0xEE && p[5] == 0xEF) {
    uint8_t buf[6] = {p[0], p[1], p[2], p[3], 0, p[5]};
    if (cksum(buf, 6) != p[4]) { Serial.println("BLE: wrong checksum"); return; }
    uint8_t cmd = p[1], d1 = p[2], d2 = p[3];
    uint16_t w = (uint16_t)(d1 << 8) | d2;
    switch (cmd) {
      case '1': cfg.strip[0] = min<uint8_t>(d2, 100); break;
      case '2': cfg.strip[1] = min<uint8_t>(d2, 100); break;
      case '3': cfg.strip[2] = min<uint8_t>(d2, 100); break;
      case 'D': cfg.durWd = d2; break;
      case 'd': cfg.durWe = d2; break;
      case 'S': cfg.switchWd = w; break;
      case 's': cfg.switchWe = w; break;
      case 'O': cfg.override_ = d2; break;
      case 'C': cfg.otaChannel = d2; break;                 // OTA-Kanal (App: V4)
      case 'U': netRequestOta(); break;                     // OTA starten (App: V4)
      default: Serial.printf("BLE: unknown cmd 0x%02x\n", cmd); return;
    }
    cfgDirty();
  } else if (n == 8 && p[0] == 0xEE && p[7] == 0xEF) {
    uint8_t buf[8] = {p[0], p[1], p[2], p[3], p[4], p[5], 0, p[7]};
    if (cksum(buf, 8) != p[6]) { Serial.println("BLE: wrong checksum"); return; }
    if (p[1] == 't') {                                      // SET_TIME: Unix-UTC, uint32 big-endian
      uint32_t t = ((uint32_t)p[2] << 24) | ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 8) | p[5];
      setSystemTime(t);
      live.timeValid = timeIsValid();
    }
  } else {
    Serial.println("BLE: wrong packet");
  }
}

// ---------------------------------------------------------------- callbacks
static String cfgString() {
  // ff06: "blink,lowWater,plusBox" (Original: "0,0,0")
  return String(cfg.blink ? 1 : 0) + "," + String(live.lowWater ? 1 : 0) + ",0";
}

class ChCb : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* c) override {
    if (c == chAp)        { String s = netScanJson(true); c->setValue((uint8_t*)s.c_str(), s.length()); }
    else if (c == chStat) { String s = netStatusJson();   c->setValue((uint8_t*)s.c_str(), s.length()); }
    else if (c == chCfg)  { String s = cfgString();       c->setValue((uint8_t*)s.c_str(), s.length()); }
    else if (c == chOta)  { String s = String(live.otaProgress) + "," + String(live.otaError); c->setValue((uint8_t*)s.c_str(), s.length()); }
    else if (c == chInfo) { String s = String(FW_VERSION) + "," + macId() + ",Standard"; c->setValue((uint8_t*)s.c_str(), s.length()); }
  }
  void onWrite(NimBLECharacteristic* c) override {
    std::string v = c->getValue();
    const uint8_t* d = (const uint8_t*)v.data();
    if (c == chCmd) {
      handlePacket(d, v.size());
    } else if (c == chAp) {                                 // "gimme" -> WLAN-Scan; Ergebnis per Read
      netRequestScan();
    } else if (c == chSet) {                                // "SSID,Passwort"
      String s(v.c_str());
      int i = s.indexOf(',');
      if (i >= 0) netRequestConnect(s.substring(0, i), s.substring(i + 1));
    } else if (c == chCfg) {                                // "1"/"0": Blinken bei wenig Wasser
      if (v.size() >= 1) { cfg.blink = (d[0] == '1'); cfgDirty(); }
      bleStatusChanged();
    } else if (c == chTz) {                                 // POSIX-TZ, nur V4 relevant, immer speichern
      String s(v.c_str());
      if (s.length() && s.length() < 64) { cfg.tz = s; applyTz(); cfgDirty(); }
    }
  }
  void onSubscribe(NimBLECharacteristic* c, ble_gap_conn_desc*, uint16_t) override {
    if (c == chCmd) cycleIdx = 0;                           // beim Abonnieren kompletten Zustand senden
  }
};
static ChCb chCb;

class SrvCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*) override {}
  void onDisconnect(NimBLEServer*) override { NimBLEDevice::startAdvertising(); }
};
static SrvCb srvCb;

// ---------------------------------------------------------------- public
void bleBegin() {
  NimBLEDevice::init("GreenBox");
  NimBLEDevice::setMTU(247);
  server = NimBLEDevice::createServer();
  server->setCallbacks(&srvCb);
  NimBLEService* svc = server->createService("00FF");
  using P = NIMBLE_PROPERTY;
  chAp   = svc->createCharacteristic("FF01", P::READ | P::WRITE | P::NOTIFY);
  chSet  = svc->createCharacteristic("FF02", P::READ | P::WRITE | P::NOTIFY);
  chStat = svc->createCharacteristic("FF03", P::READ | P::NOTIFY);
  chCmd  = svc->createCharacteristic("FF05", P::READ | P::NOTIFY | P::WRITE | P::WRITE_NR);
  chCfg  = svc->createCharacteristic("FF06", P::READ | P::WRITE | P::NOTIFY);
  chOta  = svc->createCharacteristic("FF07", P::READ);
  chInfo = svc->createCharacteristic("FF08", P::READ);
  chTz   = svc->createCharacteristic("FF09", P::WRITE);
  for (auto c : {chAp, chSet, chStat, chCmd, chCfg, chOta, chInfo, chTz}) c->setCallbacks(&chCb);
  svc->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(svc->getUUID());
  adv->setScanResponse(true);
  adv->start();
}

bool bleClients() { return server && server->getConnectedCount() > 0; }

void bleStatusChanged() {
  notifyStatus = true;
  statusRepeats = 5;
}

void bleLoop() {
  if (!bleClients()) return;
  uint32_t now = millis();
  if (now - lastCycle >= 100) {                             // Original: alle 100 ms ein Statuspaket
    lastCycle = now;
    cycleStep();
  }
  if (notifyStatus && now - lastStatusNotify >= 1000) {     // Original: ff03 nach Verbindungsversuch 5x im 1-s-Takt
    lastStatusNotify = now;
    String s = netStatusJson();
    chStat->setValue((uint8_t*)s.c_str(), s.length());
    chStat->notify();
    String c = cfgString();
    chCfg->setValue((uint8_t*)c.c_str(), c.length());
    chCfg->notify();
    if (statusRepeats && --statusRepeats == 0) notifyStatus = false;
  }
}
