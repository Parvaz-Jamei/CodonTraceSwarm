// SPDX-License-Identifier: MIT
#pragma once
#include "../SensorSample.h"
#include <Protocentral_FDC1004.h>
namespace cts {
inline Sample protocentralFdc1004Picofarads(FDC1004 &chip,
                                            fdc1004_channel_t channel,
                                            uint64_t utcMs) {
  return scalarIfFinite(chip.getCapacitancePicofarads(channel), utcMs);
}
} // namespace cts
