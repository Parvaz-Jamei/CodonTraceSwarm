// SPDX-License-Identifier: MIT
#pragma once
// Calls Adafruit BME280 Library. This is not a bench reading.
#include "../SensorSample.h"
#include <Adafruit_BME280.h>
namespace cts {
inline Sample bme280Temperature(Adafruit_BME280 &chip, uint64_t utcMs) {
  return scalarIfFinite(chip.readTemperature(), utcMs);
}
inline Sample bme280PressurePa(Adafruit_BME280 &chip, uint64_t utcMs) {
  return scalarIfFinite(chip.readPressure(), utcMs);
}
inline Sample bme280Humidity(Adafruit_BME280 &chip, uint64_t utcMs) {
  return scalarIfFinite(chip.readHumidity(), utcMs);
}
} // namespace cts
