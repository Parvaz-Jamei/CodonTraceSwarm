// SPDX-License-Identifier: MIT
#pragma once
#include "../SensorSample.h"
#include <Adafruit_SHT4x.h>
namespace cts {
inline Sample sht4xTemperature(Adafruit_SHT4x &chip, uint64_t utcMs) {
  sensors_event_t humidity, temp;
  if (!chip.getEvent(&humidity, &temp))
    return Sample::invalid("read_failed", utcMs);
  return scalarIfFinite(temp.temperature, utcMs);
}
inline Sample sht4xHumidity(Adafruit_SHT4x &chip, uint64_t utcMs) {
  sensors_event_t humidity, temp;
  if (!chip.getEvent(&humidity, &temp))
    return Sample::invalid("read_failed", utcMs);
  return scalarIfFinite(humidity.relative_humidity, utcMs);
}
} // namespace cts
