// SPDX-License-Identifier: MIT
// TEST ONLY: declaration-only fake of the vendor API that the matching
// src/cts/sensors adapter calls. Not the vendor library. Member functions are
// declared here and defined in tests/fakes/fakes_impl.h, so a symbol check of
// an adapter TU sees calls into the vendor API as undefined references only.
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int8_t VL53L0X_Error;
#define VL53L0X_ERROR_NONE ((VL53L0X_Error)0)
struct VL53L0X_RangingMeasurementData_t { uint16_t RangeMilliMeter; uint8_t RangeStatus; };
class Adafruit_VL53L0X {
public:
  VL53L0X_Error rangingTest(VL53L0X_RangingMeasurementData_t *data, bool debug);
};
