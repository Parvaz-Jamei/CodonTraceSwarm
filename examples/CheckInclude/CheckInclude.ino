#include <Arduino.h>
#include <CodonTraceSwarm.h>

// Shows that the header is visible. Does not connect, read a sensor, or send MQTT.
void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 50 && !Serial; ++i) delay(10);
  Serial.println("CodonTraceSwarm header included. No network and no sensor.");
}
void loop() {}
