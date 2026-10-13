#include <Arduino.h>
#include <CodonTraceSwarm.h>

// Shows that the header is visible. Does not connect, read a sensor, or send MQTT.
uint32_t lastPrint = 0;

void setup() { Serial.begin(115200); }
void loop() {
  uint32_t now = static_cast<uint32_t>(millis());
  if (lastPrint != 0 && static_cast<uint32_t>(now - lastPrint) < 3000) return;
  lastPrint = now;
  Serial.println("CodonTraceSwarm header included. No network and no sensor.");
}
