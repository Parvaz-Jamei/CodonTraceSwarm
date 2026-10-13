// SPDX-License-Identifier: MIT
#pragma once
#include "../SensorSample.h"
#include <Adafruit_VL53L0X.h>
namespace cts {
inline Sample vl53l0xMillimeters(Adafruit_VL53L0X &chip, uint64_t utcMs) {
  VL53L0X_RangingMeasurementData_t measure;
  if (chip.rangingTest(&measure, false) != VL53L0X_ERROR_NONE)
    return Sample::invalid("read_failed", utcMs);
  if (measure.RangeStatus != 0)
    return Sample::invalid("read_failed", utcMs);
  return scalarIfFinite(static_cast<double>(measure.RangeMilliMeter), utcMs);
}
} // namespace cts
