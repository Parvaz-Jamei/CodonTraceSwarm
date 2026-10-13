// SPDX-License-Identifier: MIT
// FDC2214 guard. Does not link FDCxCapSense. Frequencies here are synthetic.
#include "cts/Fdc2214Guard.h"
#include <cmath>
#include <cstring>
#include <iostream>
static int checks = 0, failures = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      ++failures;                                                              \
      std::cerr << "FAIL " << __LINE__ << " " #x "\n";                         \
    }                                                                          \
  } while (0)
int main() {
  using namespace cts;
  const uint32_t raw = 1u << 27;
  const double clock = 40000000.0;
  const uint16_t div2 = static_cast<uint16_t>((1u << 12) | 2u);
  auto caller = Fdc2214Guard::readFrequencyHz(Fdc2214Variant::Fdc2214, raw, clock,
                                              1, nullptr);
  auto regs = Fdc2214Guard::readFrequencyHzFromRegisters(
      Fdc2214Variant::Fdc2214, raw, clock, div2, nullptr);
  CHECK(caller.status == Fdc2214Status::Ok);
  CHECK(regs.status == Fdc2214Status::Ok);
  CHECK(caller.path != regs.path);
  CHECK(caller.path == Fdc2214Path::CallerReference);
  CHECK(regs.path == Fdc2214Path::Registers);
  CHECK(caller.hasFrequency && regs.hasFrequency);
  CHECK(caller.frequencyHz == 20000000.0);
  CHECK(regs.frequencyHz == 10000000.0);
  CHECK(caller.frequencyHz != regs.frequencyHz);
  CHECK(regs.divider == 2);
  CHECK(!caller.capacitanceDerived && !regs.capacitanceDerived);
  CHECK(std::strcmp(caller.capacitanceStatus, "capacitance_not_derived") == 0);
  CHECK(std::strcmp(regs.capacitanceStatus, "capacitance_not_derived") == 0);
  const uint16_t div0 = static_cast<uint16_t>(1u << 12);
  auto zero = Fdc2214Guard::readFrequencyHzFromRegisters(
      Fdc2214Variant::Fdc2214, raw, clock, div0, nullptr);
  const uint16_t div1 = static_cast<uint16_t>((1u << 12) | 1u);
  auto one = Fdc2214Guard::readFrequencyHzFromRegisters(
      Fdc2214Variant::Fdc2214, raw, clock, div1, nullptr);
  CHECK(zero.status == Fdc2214Status::InvalidArgument);
  CHECK(!zero.hasFrequency);
  CHECK(zero.frequencyHz == 0);
  CHECK(one.status == Fdc2214Status::Ok);
  CHECK(one.hasFrequency);
  CHECK(one.frequencyHz == 20000000.0);
  CHECK(zero.frequencyHz != one.frequencyHz);
  CHECK(std::strcmp(zero.capacitanceStatus, "capacitance_not_derived") == 0);
  auto fdc211 = Fdc2214Guard::readFrequencyHz(Fdc2214Variant::Fdc2114, raw, clock,
                                              1, nullptr);
  auto fdc211r = Fdc2214Guard::readFrequencyHzFromRegisters(
      Fdc2214Variant::Fdc2112, raw, clock, div2, nullptr);
  CHECK(fdc211.status == Fdc2214Status::NotSupported);
  CHECK(fdc211r.status == Fdc2214Status::NotSupported);
  CHECK(!fdc211.hasFrequency && !fdc211r.hasFrequency);
  CHECK(!fdc211.capacitanceDerived && !fdc211r.capacitanceDerived);
  CHECK(fdc211.frequencyHz != caller.frequencyHz);
  Fdc2214LcModel model;
  model.present = true;
  model.inductanceH = 18e-6;
  model.parasiticF = 1e-12;
  auto derived = Fdc2214Guard::readFrequencyHz(Fdc2214Variant::Fdc2212, raw, clock,
                                               1, &model);
  CHECK(derived.status == Fdc2214Status::Ok);
  CHECK(derived.hasFrequency);
  CHECK(derived.capacitanceDerived);
  CHECK(derived.capacitanceF > 0);
  CHECK(std::strcmp(derived.capacitanceStatus, "derived_from_lc_model") == 0);
  model.present = false;
  model.inductanceH = 18e-6;
  model.parasiticF = 0;
  auto absent = Fdc2214Guard::readFrequencyHz(Fdc2214Variant::Fdc2214, raw, clock,
                                              1, &model);
  CHECK(absent.hasFrequency);
  CHECK(!absent.capacitanceDerived);
  CHECK(absent.capacitanceF == 0);
  CHECK(std::strcmp(absent.capacitanceStatus, "capacitance_not_derived") == 0);
  auto none = Fdc2214Guard::readFrequencyHz(Fdc2214Variant::Fdc2214, raw, clock, 1,
                                            nullptr);
  CHECK(!none.capacitanceDerived);
  CHECK(Fdc2214Guard::readFrequencyHz(Fdc2214Variant::Fdc2214, raw, clock, 0,
                                      nullptr)
            .status == Fdc2214Status::InvalidArgument);
  CHECK(Fdc2214Guard::readFrequencyHzFromRegisters(Fdc2214Variant::Fdc2214, raw,
                                                   clock, 0, nullptr)
            .status == Fdc2214Status::InvalidArgument);
  if (failures)
    std::cerr << "checks=" << checks << " failures=" << failures << "\n";
  else
    std::cout << "PASS: FDC2214 distinct paths, divider 0, FDC211x checks="
              << checks << " failures=0\n";
  return failures ? 1 : 0;
}
