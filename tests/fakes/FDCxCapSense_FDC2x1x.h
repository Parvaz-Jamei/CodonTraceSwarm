// SPDX-License-Identifier: MIT
// TEST ONLY: declaration-only fake of the vendor API that the matching
// src/cts/sensors adapter calls. Not the vendor library. Member functions are
// declared here and defined in tests/fakes/fakes_impl.h, so a symbol check of
// an adapter TU sees calls into the vendor API as undefined references only.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "FDCxCapSense_FDC1004.h"
enum class FDC2x1xChannel { Ch0, Ch1, Ch2, Ch3 };
struct FDC2x1xFrequency { FDCxStatus status = FDCxStatus::Ok; float frequencyHz = 0; };
class FDC2x1x {
public:
  FDCxStatus readFrequencyHzFromRegisters(FDC2x1xChannel channel, float fclkHz, FDC2x1xFrequency &out);
};
