// SPDX-License-Identifier: MIT
#pragma once
#include "../SensorSample.h"
#include <HX711.h>
namespace cts {
// Does not wait. get_units() blocks until the chip is ready.
inline Sample hx711Units(HX711 &chip, uint64_t utcMs) {
  if (!chip.is_ready())
    return Sample::invalid("not_ready", utcMs);
  return scalarIfFinite(chip.get_units(1), utcMs);
}
} // namespace cts
