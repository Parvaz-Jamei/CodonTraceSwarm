// SPDX-License-Identifier: MIT
#include "Fdc2214Guard.h"
#include <cmath>
namespace cts {
namespace {
constexpr double kScale = 268435456.0; // 2^28, exact in IEEE-754 binary64
bool fdc211x(Fdc2214Variant v) noexcept {
  return v == Fdc2214Variant::Fdc2112 || v == Fdc2214Variant::Fdc2114;
}
Fdc2214Reading unsupported(Fdc2214Path path, uint32_t raw) noexcept {
  Fdc2214Reading out;
  out.status = Fdc2214Status::NotSupported;
  out.path = path;
  out.raw = raw;
  out.capacitanceStatus = "capacitance_not_derived";
  return out;
}
Fdc2214Reading invalid(Fdc2214Path path) noexcept {
  Fdc2214Reading out;
  out.status = Fdc2214Status::InvalidArgument;
  out.path = path;
  out.capacitanceStatus = "capacitance_not_derived";
  return out;
}
void derive(Fdc2214Reading &out, const Fdc2214LcModel *model) noexcept {
  out.capacitanceDerived = false;
  out.capacitanceF = 0;
  out.capacitanceStatus = "capacitance_not_derived";
  if (!out.hasFrequency || !model || !model->present)
    return;
  if (!std::isfinite(model->inductanceH) || model->inductanceH <= 0 ||
      !std::isfinite(model->parasiticF) || model->parasiticF < 0)
    return;
  constexpr double pi = 3.14159265358979323846;
  const double omega = 2.0 * pi * out.frequencyHz;
  const double total = 1.0 / (omega * omega * model->inductanceH);
  const double c = total - model->parasiticF;
  if (!std::isfinite(c) || c < 0)
    return;
  out.capacitanceDerived = true;
  out.capacitanceF = c;
  out.capacitanceStatus = "derived_from_lc_model";
}
Fdc2214Reading convert(Fdc2214Variant variant, Fdc2214Path path, uint32_t raw,
                       double referenceHz, uint16_t divider, bool requireDivider,
                       uint8_t finSel, const Fdc2214LcModel *model) noexcept {
  if (fdc211x(variant))
    return unsupported(path, raw);
  // A zero divider is an error. It is never replaced with 1.
  if (raw > 0x0FFFFFFFu || !std::isfinite(referenceHz) || referenceHz <= 0 ||
      (requireDivider && divider == 0) || (finSel != 1 && finSel != 2))
    return invalid(path);
  Fdc2214Reading out;
  out.status = Fdc2214Status::Ok;
  out.path = path;
  out.raw = raw;
  out.referenceHz = referenceHz;
  out.divider = divider;
  out.finSel = finSel;
  out.frequencyHz =
      static_cast<double>(finSel) * referenceHz * static_cast<double>(raw) / kScale;
  if (!std::isfinite(out.frequencyHz) || out.frequencyHz < 0) {
    out.hasFrequency = false;
    out.frequencyHz = 0;
    out.status = Fdc2214Status::InvalidArgument;
    out.capacitanceStatus = "capacitance_not_derived";
    return out;
  }
  out.hasFrequency = true;
  derive(out, model);
  return out;
}
} // namespace
const char *fdc2214StatusName(Fdc2214Status status) noexcept {
  switch (status) {
  case Fdc2214Status::Ok:
    return "ok";
  case Fdc2214Status::InvalidArgument:
    return "invalid_argument";
  case Fdc2214Status::NotSupported:
    return "not_supported";
  }
  return "invalid_argument";
}
Fdc2214Reading Fdc2214Guard::readFrequencyHz(Fdc2214Variant variant, uint32_t raw,
                                            double frefHz, uint8_t finSel,
                                            const Fdc2214LcModel *model) noexcept {
  // Effective fREF is already the caller-supplied reference. This path does
  // not read or invent a clock divider.
  return convert(variant, Fdc2214Path::CallerReference, raw, frefHz, 0, false,
                 finSel, model);
}
Fdc2214Reading Fdc2214Guard::readFrequencyHzFromRegisters(
    Fdc2214Variant variant, uint32_t raw, double fclkHz, uint16_t clockDividers,
    const Fdc2214LcModel *model) noexcept {
  const uint16_t divider = static_cast<uint16_t>(clockDividers & 0x03FFu);
  const unsigned finBits = (static_cast<unsigned>(clockDividers) >> 12) & 0x3u;
  uint8_t finSel = 0;
  if (finBits == 0x1u)
    finSel = 1;
  else if (finBits == 0x2u)
    finSel = 2;
  if (fdc211x(variant))
    return unsupported(Fdc2214Path::Registers, raw);
  // Reject divider 0 before any division. Do not substitute 1.
  if (divider == 0 || finSel == 0 || !std::isfinite(fclkHz) || fclkHz <= 0)
    return invalid(Fdc2214Path::Registers);
  const double fref = fclkHz / static_cast<double>(divider);
  return convert(variant, Fdc2214Path::Registers, raw, fref, divider, true, finSel,
                 model);
}
} // namespace cts
