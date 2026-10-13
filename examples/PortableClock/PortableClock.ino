#include <Arduino.h>
#include <cts/Millis32.h>
cts::Millis32 elapsed;
void setup() { elapsed.update(static_cast<uint32_t>(millis())); }
void loop() {
  // Observe regularly. This is elapsed time, not a synchronized UTC clock.
  if (!elapsed.update(static_cast<uint32_t>(millis()))) return;
}
