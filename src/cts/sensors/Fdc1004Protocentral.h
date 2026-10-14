// SPDX-License-Identifier: MIT
#pragma once
// ProtoCentral's include guard is _FDC1004. FDCxCapSense's public header
// defines FDCXCAPSENSE_FDC1004_HEADER. Both declare global class FDC1004.
// Do not include both from one sketch. Do not namespace the vendor class.
#ifdef FDCXCAPSENSE_FDC1004_HEADER
#error "the two FDC1004 libraries both declare class FDC1004 and cannot be included in one sketch"
#endif
#include "../SensorSample.h"
#include <Protocentral_FDC1004.h>
namespace cts {
inline Sample protocentralFdc1004Picofarads(FDC1004 &chip,
                                            fdc1004_channel_t channel,
                                            uint64_t utcMs) {
  return scalarIfFinite(chip.getCapacitancePicofarads(channel), utcMs);
}
} // namespace cts
