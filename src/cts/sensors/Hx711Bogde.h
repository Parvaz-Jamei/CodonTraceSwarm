// SPDX-License-Identifier: MIT
#pragma once
#include "../SensorSample.h"
#include <HX711.h>
namespace cts {
// Does not wait. get_units() blocks until the chip is ready, then divides by
// the library scale. A zero or non-finite scale is not a unit conversion.
inline Sample hx711Units(HX711 &chip, uint64_t utcMs) {
  if (!chip.is_ready())
    return Sample::invalid("not_ready", utcMs);
  const float scale = chip.get_scale();
  if (!std::isfinite(scale) || scale == 0.0f)
    return Sample::invalid("not_configured", utcMs);
  return scalarIfFinite(chip.get_units(1), utcMs);
}
} // namespace cts
