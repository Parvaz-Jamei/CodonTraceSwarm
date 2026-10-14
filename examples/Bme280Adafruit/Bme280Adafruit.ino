// Prints the reading on Serial.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Bme280Adafruit.h>

static Adafruit_BME280 chip;
static const uint8_t kBme280Address = 0x76;
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
  chipOpened = chip.begin(kBme280Address);
}

void loop() {
  if (!chipOpened) {
    Serial.println("BME280 begin_failed");
    delay(3000);
    return;
  }
  const cts::Bme280Checked reading =
      cts::bme280ReadChecked(Wire, kBme280Address, 0);
  bool first = true;
  printField(first, "temperature", reading.temperature, "degC");
  printField(first, "pressure", reading.pressure, "Pa");
  printField(first, "humidity", reading.humidity, "%");
  Serial.println();
  delay(3000);
}
