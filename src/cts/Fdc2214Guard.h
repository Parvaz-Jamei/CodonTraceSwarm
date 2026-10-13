// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
namespace cts {
// Guard for the external FDC2214 / FDC2x1x contract. It does not include,
// link, or vendor Parvaz-Jamei/FDCxCapSense.
// Pinned API: commit 215031db9a6bc37ac26cd1d2b6dca96bbe3b4da7
//   readFrequencyHz(channel, frefHz, finSel, output)
//   readFrequencyHzFromRegisters(channel, fclkHz, output)
// Those paths are distinct. Caller-reference uses the supplied effective fREF.
// The register path does fREF = fCLK / CHx_FREF_DIVIDER, with FIN_SEL in
// CLOCK_DIVIDERS bits [13:12] and the divider in bits [9:0].
// Divider 0 is an error and is never replaced by 1.
// FDC211x precise conversion is NotSupported.
// Capacitance is derived only from an explicit inductance and parasitic model.
// Without that model the result is raw/frequency only.
enum class Fdc2214Status { Ok, InvalidArgument, NotSupported };
enum class Fdc2214Variant { Fdc2212, Fdc2214, Fdc2112, Fdc2114 };
enum class Fdc2214Path { CallerReference, Registers };
struct Fdc2214LcModel {
  bool present = false;
  double inductanceH = 0;
  double parasiticF = 0;
};
struct Fdc2214Reading {
  Fdc2214Status status = Fdc2214Status::InvalidArgument;
  Fdc2214Path path = Fdc2214Path::CallerReference;
  uint32_t raw = 0;
  double frequencyHz = 0;
  double referenceHz = 0;
  uint16_t divider = 0;
  uint8_t finSel = 0;
  bool hasFrequency = false;
  bool capacitanceDerived = false;
  double capacitanceF = 0;
  const char *capacitanceStatus = "capacitance_not_derived";
};
class Fdc2214Guard {
public:
  static Fdc2214Reading readFrequencyHz(Fdc2214Variant, uint32_t raw,
                                        double frefHz, uint8_t finSel,
                                        const Fdc2214LcModel *model) noexcept;
  static Fdc2214Reading readFrequencyHzFromRegisters(
      Fdc2214Variant, uint32_t raw, double fclkHz, uint16_t clockDividers,
      const Fdc2214LcModel *model) noexcept;
};
const char *fdc2214StatusName(Fdc2214Status) noexcept;
} // namespace cts
