// SPDX-License-Identifier: MIT
// Symbol-check TU: emits src/cts/sensors/FdcxCapSense.h against declaration-only fakes.
#include "cts/sensors/FdcxCapSense.h"
namespace emit {
cts::Sample cap(FDC1004 &c, uint64_t t) { return cts::fdcxFdc1004Picofarads(c, FDC1004Measurement::M1, t); }
cts::Sample hz(FDC2x1x &c, float f, uint64_t t) { return cts::fdcxFdc2214Hertz(c, FDC2x1xChannel::Ch0, f, t); }
cts::Sample st(FDCxStatus s, float v, uint64_t t) { return cts::fdcxScalarFromStatus(s, v, t); }
} // namespace emit
