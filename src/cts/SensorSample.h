// SPDX-License-Identifier: MIT
#pragma once
#include "../CodonTraceSwarm.h"
#include <cmath>
namespace cts {
// Any sensor library can use this. A non-finite value is an invalid sample,
// never a numeric zero. utcMs is caller-supplied and is not read from the chip.
inline Sample scalarIfFinite(double value, uint64_t utcMs) {
  if (!std::isfinite(value))
    return Sample::invalid("read_failed", utcMs);
  return Sample::scalar(value, utcMs);
}
} // namespace cts
