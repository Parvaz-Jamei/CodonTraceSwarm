// Prints the reading on Serial.
#include <Arduino.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Hx711Bogde.h>

static HX711 chip;
static bool chipBegun = false;

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
  chip.begin(2, 3);
  chipBegun = true;
}

void loop() {
  if (!chipBegun) {
    Serial.println("HX711 begin_failed");
    delay(3000);
    return;
  }
  const cts::Sample sample = cts::hx711Units(chip, 0);
  printField("library_scale", sample, "scale");
  Serial.println();
  delay(3000);
}
