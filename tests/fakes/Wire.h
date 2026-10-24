// SPDX-License-Identifier: MIT
// TEST ONLY: declaration-only fake of the vendor API that the matching
// src/cts/sensors adapter calls. Not the vendor library. Member functions are
// declared here and defined in tests/fakes/fakes_impl.h, so a symbol check of
// an adapter TU sees calls into the vendor API as undefined references only.
#pragma once
#include <stddef.h>
#include <stdint.h>
class TwoWire {
public:
  void beginTransmission(uint8_t address);
  size_t write(uint8_t value);
  uint8_t endTransmission();
  size_t requestFrom(uint8_t address, size_t length);
  int available();
  int read();
};
