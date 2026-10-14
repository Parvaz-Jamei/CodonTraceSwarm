// Prints the reading on Serial.
#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Vl53l0xAdafruit.h>

static Adafruit_VL53L0X chip;
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
    Serial.println("VL53L0X begin_failed");
    delay(3000);
    return;
  }
  const cts::Sample sample = cts::vl53l0xMillimeters(chip, 0);
  printField("distance", sample, "mm");
  Serial.println();
  delay(3000);
}
