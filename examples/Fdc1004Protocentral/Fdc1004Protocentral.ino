// Prints the reading on Serial.
#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Fdc1004Protocentral.h>

static FDC1004 chip;
static bool chipOpened = false;

static void printField(const char *name, const cts::Sample &sample,
                       const char *unit) {
  Serial.print(name);
  Serial.print('=');
  if (sample.valid) {
    Serial.print(sample.value);
    Serial.print(' ');
    Serial.print(unit);
  } else {
    Serial.print(sample.status ? sample.status : "");
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  chipOpened = chip.begin();
}

void loop() {
  if (!chipOpened) {
    Serial.println("FDC1004 begin_failed");
    delay(3000);
    return;
  }
  const cts::Sample sample =
      cts::protocentralFdc1004Picofarads(chip, FDC1004_CHANNEL_0, 0);
  printField("capacitance", sample, "pF");
  Serial.println();
  delay(3000);
}
