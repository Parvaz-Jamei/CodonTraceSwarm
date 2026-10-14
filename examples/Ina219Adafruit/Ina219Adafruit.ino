// Prints the reading on Serial.
#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Ina219Adafruit.h>

static Adafruit_INA219 chip;
static bool chipOpened = false;

static void printField(bool &first, const char *name, const cts::Sample &sample,
                       const char *unit) {
  if (!first)
    Serial.print(" | ");
  first = false;
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
    Serial.println("INA219 begin_failed");
    delay(3000);
    return;
  }
  const cts::Sample shunt = cts::ina219ShuntMv(chip, 0);
  const cts::Sample bus = cts::ina219BusV(chip, 0);
  const cts::Sample current = cts::ina219CurrentMa(chip, 0);
  bool first = true;
  printField(first, "shunt", shunt, "mV");
  printField(first, "bus", bus, "V");
  printField(first, "current", current, "mA");
  Serial.println();
  delay(3000);
}
