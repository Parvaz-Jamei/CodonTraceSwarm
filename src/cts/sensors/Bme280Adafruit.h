// SPDX-License-Identifier: MIT
#pragma once
// Calls Adafruit BME280 Library. This is not a bench reading.
// A finite value from Adafruit BME280 Library 2.3.0 is not proof the I2C/SPI transaction succeeded, because read8/read16/read24 ignore the bool from write_then_read and the class has no public success().
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
