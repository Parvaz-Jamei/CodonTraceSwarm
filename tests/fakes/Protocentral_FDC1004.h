// SPDX-License-Identifier: MIT
// TEST ONLY: declaration-only fake of the vendor API that the matching
// src/cts/sensors adapter calls. Not the vendor library. Member functions are
// declared here and defined in tests/fakes/fakes_impl.h, so a symbol check of
// an adapter TU sees calls into the vendor API as undefined references only.
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef uint8_t fdc1004_channel_t;
class FDC1004 {
public:
  float getCapacitancePicofarads(fdc1004_channel_t channel);
};
