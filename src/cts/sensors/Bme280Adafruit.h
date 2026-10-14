// SPDX-License-Identifier: MIT
#pragma once
// Does not call Adafruit_BME280::readTemperature, readPressure, or readHumidity.
// In Adafruit BME280 Library 2.3.0 those getters can return a finite float after
// a dead bus: read8/read16/read24 ignore the bool from write_then_read.
// Calibration (0x88 and 0xE1) and measurement (0xF7) are read with TwoWire
// endTransmission and requestFrom. A non-zero transmission or a short read is
// Sample::invalid("read_failed", utcMs) and is not converted. Chip id is not
// used as proof. This is not a bench reading. utcMs is caller-supplied.
#include "Bme280Comp.h"
#include <Wire.h>
namespace cts {
inline bool bme280WireRead(TwoWire &wire, uint8_t address, uint8_t reg,
                           uint8_t *dest, size_t len) {
  if (dest == nullptr || len == 0 || len > 255)
    return false;
  wire.beginTransmission(address);
  const size_t wrote = wire.write(reg);
  const uint8_t transmission = wire.endTransmission();
  if (wrote != 1 || transmission != 0)
    return false;
  const size_t got = wire.requestFrom(address, len);
  if (got < len)
    return false;
  for (size_t i = 0; i < len; ++i) {
    if (wire.available() <= 0)
      return false;
    const int byte = wire.read();
    if (byte < 0)
      return false;
    dest[i] = static_cast<uint8_t>(byte);
  }
  return true;
}
struct Bme280Checked {
  Sample temperature;
  Sample pressure;
  Sample humidity;
};
inline Bme280Checked bme280ReadChecked(TwoWire &wire, uint8_t address,
                                      uint64_t utcMs) {
  uint8_t tempPress[26];
  uint8_t humidity[7];
  uint8_t measurement[8];
  const bool tempPressOk = bme280WireRead(wire, address, 0x88, tempPress, 26);
  const bool humidityOk =
      tempPressOk && bme280WireRead(wire, address, 0xE1, humidity, 7);
  const bool measurementOk =
      humidityOk && bme280WireRead(wire, address, 0xF7, measurement, 8);
  Bme280Checked out;
  if (!measurementOk) {
    // Do not compensate. The pressure formula's 0 is not this failure.
    out.temperature = bme280ShortReadSample(0, 26, 0.0, utcMs);
    out.pressure = bme280ShortReadSample(0, 8, 0.0, utcMs);
    out.humidity = bme280ShortReadSample(0, 7, 0.0, utcMs);
    return out;
  }
  const Bme280Samples samples = bme280SamplesFromRegisters(
      tempPress, 26, humidity, 7, measurement, 8, utcMs);
  out.temperature = samples.temperature;
  out.pressure = samples.pressure;
  out.humidity = samples.humidity;
  return out;
}
} // namespace cts
