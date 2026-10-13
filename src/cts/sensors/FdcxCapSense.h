// SPDX-License-Identifier: MIT
#pragma once
#include "../SensorSample.h"
#include <FDCxCapSense_FDC1004.h>
#include <FDCxCapSense_FDC2x1x.h>
namespace cts {
// A non-Ok status is never a sample, even when value is a leftover 0 or some
// other finite number from an earlier attempt.
inline Sample fdcxScalarFromStatus(FDCxStatus status, float value, uint64_t utcMs) {
  if (status == FDCxStatus::MeasurementNotReady)
    return Sample::invalid("not_ready", utcMs);
  if (status != FDCxStatus::Ok)
    return Sample::invalid("read_failed", utcMs);
  return scalarIfFinite(static_cast<double>(value), utcMs);
}
inline Sample fdcxFdc1004Picofarads(FDC1004 &chip, FDC1004Measurement measurement,
                                    uint64_t utcMs) {
  float picofarads = 0;
  const FDCxStatus status = chip.readCapacitancePf(measurement, picofarads);
  return fdcxScalarFromStatus(status, picofarads, utcMs);
}
inline Sample fdcxFdc2214Hertz(FDC2x1x &chip, FDC2x1xChannel channel, float fclkHz,
                               uint64_t utcMs) {
  // Caller supplies fclk. Do not invent a clock and do not touch the chip
  // until that argument is a positive finite frequency.
  if (!std::isfinite(fclkHz) || fclkHz <= 0.0f)
    return Sample::invalid("invalid_argument", utcMs);
  FDC2x1xFrequency reading;
  const FDCxStatus status =
      chip.readFrequencyHzFromRegisters(channel, fclkHz, reading);
  if (status == FDCxStatus::MeasurementNotReady)
    return Sample::invalid("not_ready", utcMs);
  if (status != FDCxStatus::Ok)
    return Sample::invalid("read_failed", utcMs);
  return fdcxScalarFromStatus(reading.status, reading.frequencyHz, utcMs);
}
} // namespace cts
