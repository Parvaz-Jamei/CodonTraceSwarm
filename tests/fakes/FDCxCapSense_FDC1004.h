// SPDX-License-Identifier: MIT
// TEST ONLY: declaration-only fake of the vendor API that the matching
// src/cts/sensors adapter calls. Not the vendor library. Member functions are
// declared here and defined in tests/fakes/fakes_impl.h, so a symbol check of
// an adapter TU sees calls into the vendor API as undefined references only.
#pragma once
#include <stddef.h>
#include <stdint.h>
enum class FDCxStatus { Ok, MeasurementNotReady, BusError };
enum class FDC1004Measurement { M1, M2, M3, M4 };
class FDC1004 {
public:
  FDCxStatus readCapacitancePf(FDC1004Measurement measurement, float &picofarads);
};
