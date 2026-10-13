// SPDX-License-Identifier: MIT
#pragma once
#include "../SensorSample.h"
#include <Adafruit_INA219.h>
namespace cts {
inline Sample ina219ShuntMv(Adafruit_INA219 &chip, uint64_t utcMs) {
  return scalarIfFinite(chip.getShuntVoltage_mV(), utcMs);
}
inline Sample ina219BusV(Adafruit_INA219 &chip, uint64_t utcMs) {
  return scalarIfFinite(chip.getBusVoltage_V(), utcMs);
}
inline Sample ina219CurrentMa(Adafruit_INA219 &chip, uint64_t utcMs) {
  return scalarIfFinite(chip.getCurrent_mA(), utcMs);
}
} // namespace cts
