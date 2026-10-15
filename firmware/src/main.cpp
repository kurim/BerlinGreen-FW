#include "gb.h"

void setup() {
  Serial.begin(115200);
  Serial.println("\nGreenBox custom firmware " FW_VERSION);
  cfgLoad();
  lightBegin();
  bleBegin();
  netBegin();
  webBegin();
  mqttBegin();
}

void loop() {
  lightLoop();
  bleLoop();
  netLoop();
  webLoop();
  mqttLoop();
  cfgLoop();
  delay(2);
}
