// Leistungsschaetzung (W, Netzseite) aus den drei Streifenwerten 0..100 %. Header-only, auch fuer ESPHome (includes:).
// Grundlage: Messreihe Flex 2 an 230 V (firmware/power-measurement.md). Die Box kennt keine Farbtemperatur (nur die App
// rechnet sie in Streifenwerte um, siehe PROTOCOL.md); der Verbrauch addiert sich je Streifen (4500 K = Summe der drei
// Streifen, Abweichung zu allen Messpunkten <= 1 W). Stuetzstellen alle 10 % Streifenwert, dazwischen linear,
// ausserhalb 0..100 geklemmt. Werte = Messung minus Ruheleistung.
#pragma once

// Ruheleistung der Box bei 0 % (4500 K: 0,5 W; 3000 K 1,2 W, 3700 K 0,6 W, 6000 K 0,4 W - uneinheitlich, siehe Doku)
static const float GB_POWER_IDLE_W = 0.5f;
static const float GB_POWER_STRIPE_W[3][11] = {
  {0, 0.5f, 1.3f, 2.2f, 2.7f, 3.6f, 4.2f, 4.9f, 5.8f, 6.6f, 7.4f},   // Streifen 1 warm    (aus 3000 K)
  {0, 1.7f, 2.2f, 2.8f, 3.7f, 4.0f, 5.1f, 5.6f, 6.2f, 6.8f, 7.7f},   // Streifen 2 neutral (aus 3700 K minus Streifen 1)
  {0, 0.95f, 1.9f, 2.65f, 3.4f, 4.15f, 4.9f, 5.55f, 6.2f, 7.05f, 7.9f},  // Streifen 3 kalt    (aus 6000 K, 20-%-Raster, Mitten linear)
};

static inline float gbPowerW(const float pct[3]) {
  float w = GB_POWER_IDLE_W;
  for (int i = 0; i < 3; i++) {
    float p = pct[i] < 0 ? 0 : pct[i] > 100 ? 100 : pct[i];
    int k = (int)(p / 10);
    if (k > 9) k = 9;
    w += GB_POWER_STRIPE_W[i][k] + (GB_POWER_STRIPE_W[i][k + 1] - GB_POWER_STRIPE_W[i][k]) * (p / 10 - k);
  }
  return w;
}
