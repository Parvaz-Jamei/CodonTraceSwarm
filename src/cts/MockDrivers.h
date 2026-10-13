// SPDX-License-Identifier: MIT
#pragma once
#include "RegisterBus.h"
#include "../CodonTraceSwarm.h"
namespace cts {
// Optional mock drivers. Register images are SYNTHETIC. They are not silicon
// maps, not calibrated, and not bench results. Failures are invalid samples:
// never NaN and never a valid numeric zero. No Wire.h, heap, String, or threads.
enum class DriverStatus {
  Ok,
  NotReady,
  ReadFailed,
  Unsupported,
  InvalidArgument,
  AlreadyApplied,
  NotConfigured
};
struct Bme280Reading {
  DriverStatus status = DriverStatus::ReadFailed;
  Sample temperature, pressure, humidity;
};
class Bme280 {
public:
  Bme280(ByteBus &bus, uint8_t address) : bus_(bus), address_(address) {}
  Bme280Reading read(uint64_t utcMs) noexcept;

private:
  ByteBus &bus_;
  uint8_t address_;
};
struct Sht4xView {
  bool bound = false;
  char quantity[16] = {};
};
struct Sht4xReading {
  DriverStatus status = DriverStatus::ReadFailed;
  Sample temperature, humidity;
};
class Sht4x {
public:
  Sht4x(ByteBus &bus, uint8_t address) : bus_(bus), address_(address) {}
  // "pressure" is Unsupported. Temperature and humidity are the only channels.
  static DriverStatus bind(const char *quantity, Sht4xView &view) noexcept;
  DriverStatus readQuantity(const char *quantity, Sample &out, uint64_t utcMs) noexcept;
  Sht4xReading read(uint64_t utcMs) noexcept;

private:
  ByteBus &bus_;
  uint8_t address_;
};
struct Ina219Reading {
  DriverStatus status = DriverStatus::ReadFailed;
  bool hasCounts = false;
  int16_t shuntCounts = 0, busCounts = 0;
  Sample shunt, bus;
  bool hasCurrent = false;
  Sample current;
};
class Ina219 {
public:
  Ina219(ByteBus &bus, uint8_t address) : bus_(bus), address_(address) {}
  // ohms must be finite and > 0. Until then, reads expose counts only.
  DriverStatus setShuntOhms(double ohms) noexcept;
  bool shuntConfigured() const noexcept { return shuntSet_; }
  Ina219Reading read(uint64_t utcMs) noexcept;

private:
  ByteBus &bus_;
  uint8_t address_;
  double shuntOhms_ = 0;
  bool shuntSet_ = false;
};
struct Vl53l0xReading {
  DriverStatus status = DriverStatus::ReadFailed;
  bool rangeValid = false;
  uint16_t millimeters = 0;
  Sample distance;
};
class Vl53l0x {
public:
  Vl53l0x(ByteBus &bus, uint8_t address) : bus_(bus), address_(address) {}
  Vl53l0xReading read(uint64_t utcMs) noexcept;

private:
  ByteBus &bus_;
  uint8_t address_;
};
struct Hx711Reading {
  DriverStatus status = DriverStatus::ReadFailed;
  bool hasRaw = false;
  int32_t raw = 0;
  Sample rawSample;
  bool hasKilograms = false;
  Sample kilograms;
};
class Hx711 {
public:
  Hx711(ByteBus &bus, uint8_t address) : bus_(bus), address_(address) {}
  DriverStatus setTare(int32_t counts) noexcept;
  DriverStatus setSpanCountsPerKilogram(double span) noexcept;
  bool calibrated() const noexcept { return tareSet_ && spanSet_; }
  Hx711Reading read(uint64_t utcMs) noexcept;

private:
  ByteBus &bus_;
  uint8_t address_;
  int32_t tare_ = 0;
  double span_ = 0;
  bool tareSet_ = false, spanSet_ = false;
};
struct Fdc1004Reading {
  DriverStatus status = DriverStatus::ReadFailed;
  bool hasRaw = false;
  int32_t raw = 0;
  bool capdacKnown = false;
  uint8_t capdac = 0;
  Sample rawSample;
  bool hasPicofarads = false;
  Sample picofarads;
};
class Fdc1004 {
public:
  Fdc1004(ByteBus &bus, uint8_t address) : bus_(bus), address_(address) {}
  // A second successful apply on the same channel is AlreadyApplied.
  DriverStatus applyCapdac(uint8_t channel, uint8_t capdac) noexcept;
  bool capdacApplied(uint8_t channel) const noexcept;
  uint8_t capdac(uint8_t channel) const noexcept;
  uint32_t rejectedCapdacApplies() const noexcept { return rejected_; }
  Fdc1004Reading read(uint8_t channel, uint64_t utcMs) noexcept;

private:
  ByteBus &bus_;
  uint8_t address_;
  uint8_t capdac_[4] = {};
  bool applied_[4] = {};
  uint32_t rejected_ = 0;
};
const char *driverStatusName(DriverStatus) noexcept;
} // namespace cts
