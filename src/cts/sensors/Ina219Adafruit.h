// SPDX-License-Identifier: MIT
#pragma once
#include "../SensorSample.h"
#include <Adafruit_INA219.h>
namespace cts {
// The float getters do not report an I2C miss. success() is the explicit
// failure from the installed Adafruit library. A finite zero stays valid.
inline Sample ina219AfterRead(float value, bool readOk, uint64_t utcMs) {
  if (!readOk)
    return Sample::invalid("read_failed", utcMs);
  return scalarIfFinite(value, utcMs);
}
inline Sample ina219ShuntMv(Adafruit_INA219 &chip, uint64_t utcMs) {
  const float value = chip.getShuntVoltage_mV();
  return ina219AfterRead(value, chip.success(), utcMs);
}
inline Sample ina219BusV(Adafruit_INA219 &chip, uint64_t utcMs) {
  const float value = chip.getBusVoltage_V();
  return ina219AfterRead(value, chip.success(), utcMs);
}
inline Sample ina219CurrentMa(Adafruit_INA219 &chip, uint64_t utcMs) {
  const float value = chip.getCurrent_mA();
  return ina219AfterRead(value, chip.success(), utcMs);
}
} // namespace cts
