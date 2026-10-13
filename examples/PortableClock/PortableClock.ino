#include <Arduino.h>
#include <CodonTraceSwarm.h>
#include <cts/Millis32.h>

cts::Millis32 elapsed;
uint32_t lastPrint = 0;
bool reportedFault = false;

static void printU64(uint64_t value) {
  char buf[21];
  char *end = buf + sizeof(buf) - 1;
  *end = 0;
  do {
    *--end = static_cast<char>('0' + static_cast<unsigned>(value % 10));
    value /= 10;
  } while (value);
  Serial.print(end);
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 50 && !Serial; ++i) delay(10);
  elapsed.update(static_cast<uint32_t>(millis()));
  Serial.println("CodonTraceSwarm elapsed milliseconds. Not UTC and not synchronized.");
}
void loop() {
  uint32_t now = static_cast<uint32_t>(millis());
  if (!elapsed.update(now)) {
    if (!reportedFault) {
      reportedFault = true;
      Serial.println("CodonTraceSwarm clock fault. Elapsed time stopped. Not UTC.");
    }
    return;
  }
  if (static_cast<uint32_t>(now - lastPrint) < 1000) return;
  lastPrint = now;
  Serial.print("elapsed_ms=");
  printU64(elapsed.elapsedMs());
  Serial.println(" not UTC");
}
