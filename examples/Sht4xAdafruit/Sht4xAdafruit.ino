// Prints the reading on Serial.
#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Sht4xAdafruit.h>

static Adafruit_SHT4x chip;
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
    Serial.println("SHT4x begin_failed");
    delay(3000);
    return;
  }
  const cts::Sample temperature = cts::sht4xTemperature(chip, 0);
  const cts::Sample humidity = cts::sht4xHumidity(chip, 0);
  bool first = true;
  printField(first, "temperature", temperature, "degC");
  printField(first, "humidity", humidity, "%");
  Serial.println();
  delay(3000);
}
