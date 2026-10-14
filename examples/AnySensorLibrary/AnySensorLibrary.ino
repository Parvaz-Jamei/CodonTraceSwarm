// Prints the reading on Serial.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <CodonTraceSwarm.h>
#include <cts/SensorSample.h>

static Adafruit_BME280 chip;
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
  chipOpened = chip.begin(0x76);
}

void loop() {
  if (!chipOpened) {
    Serial.println("BME280 begin_failed");
    delay(3000);
    return;
  }
  const double temperatureFromLibrary = chip.readTemperature();
  const double humidityFromLibrary = chip.readHumidity();
  const cts::Sample temperature = cts::scalarIfFinite(temperatureFromLibrary, 0);
  const cts::Sample humidity = cts::scalarIfFinite(humidityFromLibrary, 0);
  bool first = true;
  printField(first, "temperature", temperature, "degC");
  printField(first, "humidity", humidity, "%");
  Serial.println();
  delay(3000);
}
