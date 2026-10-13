// SPDX-License-Identifier: MIT
#pragma once
#include "../SensorSample.h"
#include <FDCxCapSense_FDC1004.h>
#include <FDCxCapSense_FDC2x1x.h>
namespace cts {
inline Sample fdcxFdc1004Picofarads(FDC1004 &chip, FDC1004Measurement measurement,
                                    uint64_t utcMs) {
  float picofarads = 0;
  if (chip.readCapacitancePf(measurement, picofarads) != FDCxStatus::Ok)
    return Sample::invalid("read_failed", utcMs);
  return scalarIfFinite(picofarads, utcMs);
}
inline Sample fdcxFdc2214Hertz(FDC2x1x &chip, FDC2x1xChannel channel, float fclkHz,
                               uint64_t utcMs) {
  FDC2x1xFrequency reading;
  if (chip.readFrequencyHzFromRegisters(channel, fclkHz, reading) != FDCxStatus::Ok)
    return Sample::invalid("read_failed", utcMs);
  if (reading.status != FDCxStatus::Ok)
    return Sample::invalid("read_failed", utcMs);
  return scalarIfFinite(reading.frequencyHz, utcMs);
}
} // namespace cts
