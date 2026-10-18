// LED-Streifen, Zeitplan, Wasserstand, Taster - Logik aus der Original-Firmware (Ghidra: FUN_400d2c48/2d58/3138/3288).
#include "gb.h"
#include <time.h>

static const uint8_t PIN_STRIPE[3] = {23, 22, 21};   // LEDC ch0..2
static const uint8_t PIN_WATER[3]  = {14, 27, 13};   // Touch T6, T7, T4
static const uint8_t PIN_BUTTON    = 12;             // Touch T5
static const int     DUTY_MAX      = 1440;           // 0x5a0
static const int     FADE_STEP     = 27;             // 0x1b

static uint16_t cur[3] = {0, 0, 0};
static bool     on = false;
static uint32_t lastTick = 0, lastWater = 0, dipUntil = 0, lockUntil = 0;
static float    btnBaseF = 0;           // nachlaufende Baseline (EMA)
static uint32_t lastBtn = 0;
static uint8_t  btnLow = 0;             // aufeinanderfolgende Abtastungen unter der Schwelle

static int rawFor(uint8_t pct) {
  if (!pct) return 0;
  return (int)pct * 1340 / 100 + 100;      // wie Original: pct*0x53c/100, +100 Offset
}

static uint16_t touchMedian(uint8_t pin, int n = 11) {
  uint16_t v[11];
  for (int i = 0; i < n; i++) { v[i] = touchRead(pin); delayMicroseconds(100); }
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && v[j - 1] > v[j]; j--) { uint16_t t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
  return v[n / 2];
}

bool lightIsOn() { return on; }

void lightBegin() {
  for (int i = 0; i < 3; i++) {
    ledcSetup(i, 25000, 11);               // 25 kHz, 11 Bit
    ledcAttachPin(PIN_STRIPE[i], i);
    ledcWrite(i, 0);
  }
  btnBaseF = touchMedian(PIN_BUTTON);
  live.btnBase = (uint16_t)btnBaseF;
}

void lightToggleOverride() {
  // wie Original (FUN_400d3288): 3 -> 1, 1 -> 0, 0 -> 1; sind alle Streifen 0, werden sie auf 100 gesetzt
  if (cfg.override_ == 3) cfg.override_ = 0;
  cfg.override_ = (cfg.override_ == 0) ? 1 : 0;
  if (!cfg.strip[0] && !cfg.strip[1] && !cfg.strip[2]) cfg.strip[0] = cfg.strip[1] = cfg.strip[2] = 100;
  cfgDirty();
}

// Zeitplanfenster: heute und gestern pruefen (Fenster ueber Mitternacht), Wochenende nur wenn Dauer-WE != 0.
static bool scheduleOn(const struct tm& t) {
  int nowMin = t.tm_hour * 60 + t.tm_min;
  for (int back = 0; back <= 1; back++) {
    int wd = (t.tm_wday + 7 - back) % 7;               // 0 = Sonntag
    bool we = (wd == 0 || wd == 6) && cfg.durWe != 0;
    uint16_t sw = we ? cfg.switchWe : cfg.switchWd;
    int dur = we ? cfg.durWe : cfg.durWd;
    int start = (sw / 100) * 60 + sw % 100;
    int rel = nowMin + back * 1440;
    if (dur > 0 && rel >= start && rel < start + dur * 60) return true;
  }
  return false;
}

static void waterCheck() {
  uint16_t a = touchMedian(PIN_WATER[0]), b = touchMedian(PIN_WATER[1]), c = touchMedian(PIN_WATER[2]);
  live.touch[0] = a; live.touch[1] = b; live.touch[2] = c;
  uint8_t lvl;
  if (a > 9 || b < 10 || c < 10) {            // Logik 1:1 aus FUN_400d3138
    if (a > 9 || b > 9) lvl = 0;
    else if (c < 10) lvl = 100;
    else lvl = 50;
  } else lvl = 20;
  live.water = lvl;
  live.lowWater = (lvl <= 20 && cfg.blink);
  if (live.lowWater && cfg.override_ != 0 && on) dipUntil = millis() + 700;   // kurzes Abdunkeln = "Blinken"
}

void lightLoop() {
  uint32_t now = millis();

  // Taster (Touch GPIO12): Original entprellt ~1,5 s
  // Touch-Taster IO12 (alle 50 ms abtasten). Baseline folgt langsam (EMA), damit Beruehren beim Boot oder
  // Temperatur-/Feuchtedrift die Erkennung nicht verschieben. Ausloesung: Wert < Baseline - delta (Original: 5).
  if (now - lastBtn >= 50) {
    lastBtn = now;
    uint16_t v = touchRead(PIN_BUTTON);
    live.touch[3] = v;
    bool below = v + cfg.btnDelta < btnBaseF;
    btnLow = below ? (btnLow < 255 ? btnLow + 1 : 255) : 0;
    bool pressed = btnLow >= 3;               // 3 Abtastungen (150 ms) in Folge: einzelne Ausreisser loesen nicht aus
    if (!below) btnBaseF += ((float)v - btnBaseF) / 32.0f;
    live.btnBase = (uint16_t)(btnBaseF + 0.5f);
    if (pressed && !live.btnPressed && cfg.button && now > lockUntil) {   // nur steigende Flanke
      lightToggleOverride();
      lockUntil = now + 1500;
    }
    live.btnPressed = pressed;
  }

  if (now - lastWater >= 5000 || lastWater == 0) { lastWater = now; waterCheck(); }

  if (now - lastTick < 20) return;           // Original: alle 20 ms ein Fade-Schritt
  lastTick = now;

  live.timeValid = timeIsValid();
  bool breathe = false;
  if (cfg.override_ == 1) on = true;
  else if (cfg.override_ == 0) on = false;
  else if (!live.timeValid) { on = false; breathe = true; }   // Original: "Light schedule skipped since time is not set" + Atmen
  else {
    time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    on = scheduleOn(tm);
  }

  for (int i = 0; i < 3; i++) {
    int target = on ? rawFor(cfg.strip[i]) : 0;
    if (on && now < dipUntil) target = 0;
    if (breathe && i == 0) {                 // Dreieck auf Streifen 1, ~6 s Periode, max 350
      uint32_t ph = now % 6000;
      target = (ph < 3000 ? ph : 6000 - ph) * 350 / 3000;
    }
    if (target > DUTY_MAX) target = DUTY_MAX;
    int c = cur[i];
    if (c < target) c = min(c + FADE_STEP, target);
    else if (c > target) c = max(c - FADE_STEP, target);
    if (c != cur[i]) { cur[i] = c; ledcWrite(i, c); }
  }
}
