// SPDX-License-Identifier: MIT
#include "MockDrivers.h"
#include <cmath>
#include <cstring>
namespace cts {
namespace {
Sample bad(const char *why, uint64_t utc) noexcept {
  return Sample::invalid(why, utc);
}
BusStatus readAt(ByteBus &bus, uint8_t address, uint8_t reg, uint8_t *dst,
                 size_t n) noexcept {
  const uint8_t pointer = reg;
  if (bus.write(address, &pointer, 1) != BusStatus::Ok)
    return BusStatus::Fail;
  return bus.read(address, dst, n);
}
int32_t le32(const uint8_t *b) noexcept {
  const uint32_t u = static_cast<uint32_t>(b[0]) |
                     (static_cast<uint32_t>(b[1]) << 8) |
                     (static_cast<uint32_t>(b[2]) << 16) |
                     (static_cast<uint32_t>(b[3]) << 24);
  return static_cast<int32_t>(u);
}
int16_t be16(const uint8_t *b) noexcept {
  const uint16_t u = static_cast<uint16_t>((static_cast<uint16_t>(b[0]) << 8) |
                                           b[1]);
  return static_cast<int16_t>(u);
}
int32_t sign24(const uint8_t *b) noexcept {
  const uint32_t u = (static_cast<uint32_t>(b[0]) << 16) |
                     (static_cast<uint32_t>(b[1]) << 8) |
                     static_cast<uint32_t>(b[2]);
  if (u & 0x800000u)
    return static_cast<int32_t>(u | 0xFF000000u);
  return static_cast<int32_t>(u);
}
bool finite(double v) noexcept { return std::isfinite(v); }
} // namespace
const char *driverStatusName(DriverStatus status) noexcept {
  switch (status) {
  case DriverStatus::Ok:
    return "ok";
  case DriverStatus::NotReady:
    return "not_ready";
  case DriverStatus::ReadFailed:
    return "read_failed";
  case DriverStatus::Unsupported:
    return "unsupported";
  case DriverStatus::InvalidArgument:
    return "invalid_argument";
  case DriverStatus::AlreadyApplied:
    return "already_applied";
  case DriverStatus::NotConfigured:
    return "not_configured";
  }
  return "invalid_argument";
}
Bme280Reading Bme280::read(uint64_t utcMs) noexcept {
  // Synthetic map, not a Bosch image: 0xD0 id 0x60, 0xF3 bit0 not-ready,
  // 0x10 three little-endian int32 values (milli-degC, Pa, milli-percent RH).
  Bme280Reading out;
  out.temperature = bad("read_failed", utcMs);
  out.pressure = bad("read_failed", utcMs);
  out.humidity = bad("read_failed", utcMs);
  uint8_t id = 0, status = 0, raw[12];
  if (readAt(bus_, address_, 0xD0, &id, 1) != BusStatus::Ok || id != 0x60) {
    out.status = DriverStatus::ReadFailed;
    return out;
  }
  if (readAt(bus_, address_, 0xF3, &status, 1) != BusStatus::Ok) {
    out.status = DriverStatus::ReadFailed;
    return out;
  }
  if (status & 0x01u) {
    out.status = DriverStatus::NotReady;
    out.temperature = bad("not_ready", utcMs);
    out.pressure = bad("not_ready", utcMs);
    out.humidity = bad("not_ready", utcMs);
    return out;
  }
  if (readAt(bus_, address_, 0x10, raw, sizeof(raw)) != BusStatus::Ok) {
    out.status = DriverStatus::ReadFailed;
    return out;
  }
  const double t = static_cast<double>(le32(raw)) / 1000.0;
  const double p = static_cast<double>(le32(raw + 4));
  const double h = static_cast<double>(le32(raw + 8)) / 1000.0;
  if (!finite(t) || !finite(p) || !finite(h)) {
    out.status = DriverStatus::ReadFailed;
    return out;
  }
  out.status = DriverStatus::Ok;
  out.temperature = Sample::scalar(t, utcMs);
  out.pressure = Sample::scalar(p, utcMs);
  out.humidity = Sample::scalar(h, utcMs);
  return out;
}
DriverStatus Sht4x::bind(const char *quantity, Sht4xView &view) noexcept {
  view.bound = false;
  view.quantity[0] = 0;
  if (!quantity)
    return DriverStatus::InvalidArgument;
  if (!std::strcmp(quantity, "pressure"))
    return DriverStatus::Unsupported;
  if (std::strcmp(quantity, "temperature") && std::strcmp(quantity, "humidity"))
    return DriverStatus::InvalidArgument;
  std::memcpy(view.quantity, quantity, std::strlen(quantity) + 1);
  view.bound = true;
  return DriverStatus::Ok;
}
Sht4xReading Sht4x::read(uint64_t utcMs) noexcept {
  Sht4xReading out;
  out.temperature = bad("read_failed", utcMs);
  out.humidity = bad("read_failed", utcMs);
  uint8_t raw[6];
  if (readAt(bus_, address_, 0xFD, raw, sizeof(raw)) != BusStatus::Ok) {
    out.status = DriverStatus::ReadFailed;
    return out;
  }
  const double traw = (static_cast<unsigned>(raw[0]) << 8) | raw[1];
  const double hraw = (static_cast<unsigned>(raw[3]) << 8) | raw[4];
  const double t = -45.0 + 175.0 * traw / 65535.0;
  const double h = -6.0 + 125.0 * hraw / 65535.0;
  if (!finite(t) || !finite(h)) {
    out.status = DriverStatus::ReadFailed;
    return out;
  }
  out.status = DriverStatus::Ok;
  out.temperature = Sample::scalar(t, utcMs);
  out.humidity = Sample::scalar(h, utcMs);
  return out;
}
DriverStatus Sht4x::readQuantity(const char *quantity, Sample &out,
                                uint64_t utcMs) noexcept {
  Sht4xView view;
  const auto bound = bind(quantity, view);
  if (bound != DriverStatus::Ok) {
    out = bad(bound == DriverStatus::Unsupported ? "unsupported"
                                                 : "invalid_argument",
              utcMs);
    return bound;
  }
  const auto reading = read(utcMs);
  if (reading.status != DriverStatus::Ok) {
    out = bad("read_failed", utcMs);
    return reading.status;
  }
  out = view.quantity[0] == 'h' ? reading.humidity : reading.temperature;
  return DriverStatus::Ok;
}
DriverStatus Ina219::setShuntOhms(double ohms) noexcept {
  if (!finite(ohms) || ohms <= 0)
    return DriverStatus::InvalidArgument;
  shuntOhms_ = ohms;
  shuntSet_ = true;
  return DriverStatus::Ok;
}
Ina219Reading Ina219::read(uint64_t utcMs) noexcept {
  // Synthetic non-overlapping counts: 0x10 int16 BE shunt, 0x12 int16 BE bus.
  // Amperes use a 10 µV LSB only after setShuntOhms. This is not a calibration.
  Ina219Reading out;
  out.shunt = bad("read_failed", utcMs);
  out.bus = bad("read_failed", utcMs);
  out.current = bad("not_configured", utcMs);
  uint8_t shunt[2], bus[2];
  if (readAt(bus_, address_, 0x10, shunt, 2) != BusStatus::Ok ||
      readAt(bus_, address_, 0x12, bus, 2) != BusStatus::Ok) {
    out.status = DriverStatus::ReadFailed;
    out.current = bad("read_failed", utcMs);
    return out;
  }
  out.hasCounts = true;
  out.shuntCounts = be16(shunt);
  out.busCounts = be16(bus);
  out.shunt = Sample::scalar(static_cast<double>(out.shuntCounts), utcMs);
  out.bus = Sample::scalar(static_cast<double>(out.busCounts), utcMs);
  if (!shuntSet_) {
    out.status = DriverStatus::NotConfigured;
    out.hasCurrent = false;
    out.current = bad("not_configured", utcMs);
    return out;
  }
  const double amperes =
      static_cast<double>(out.shuntCounts) * 10e-6 / shuntOhms_;
  if (!finite(amperes)) {
    out.status = DriverStatus::ReadFailed;
    out.hasCurrent = false;
    out.current = bad("read_failed", utcMs);
    return out;
  }
  out.status = DriverStatus::Ok;
  out.hasCurrent = true;
  out.current = Sample::scalar(amperes, utcMs);
  return out;
}
Vl53l0xReading Vl53l0x::read(uint64_t utcMs) noexcept {
  // Synthetic: 0x00 status (0 valid, 1 not ready, 2 timeout), 0x02 uint16 LE mm.
  Vl53l0xReading out;
  out.distance = bad("read_failed", utcMs);
  uint8_t status = 0, mm[2];
  if (readAt(bus_, address_, 0x00, &status, 1) != BusStatus::Ok) {
    out.status = DriverStatus::ReadFailed;
    return out;
  }
  if (status == 1 || status == 2) {
    out.status = status == 1 ? DriverStatus::NotReady : DriverStatus::ReadFailed;
    out.rangeValid = false;
    out.distance = bad(status == 1 ? "not_ready" : "timeout", utcMs);
    return out;
  }
  if (status != 0 || readAt(bus_, address_, 0x02, mm, 2) != BusStatus::Ok) {
    out.status = DriverStatus::ReadFailed;
    return out;
  }
  out.millimeters =
      static_cast<uint16_t>((static_cast<uint16_t>(mm[1]) << 8) | mm[0]);
  out.rangeValid = true;
  out.status = DriverStatus::Ok;
  out.distance = Sample::scalar(static_cast<double>(out.millimeters), utcMs);
  return out;
}
DriverStatus Hx711::setTare(int32_t counts) noexcept {
  tare_ = counts;
  tareSet_ = true;
  return DriverStatus::Ok;
}
DriverStatus Hx711::setSpanCountsPerKilogram(double span) noexcept {
  if (!finite(span) || span == 0)
    return DriverStatus::InvalidArgument;
  span_ = span;
  spanSet_ = true;
  return DriverStatus::Ok;
}
Hx711Reading Hx711::read(uint64_t utcMs) noexcept {
  Hx711Reading out;
  out.rawSample = bad("read_failed", utcMs);
  out.kilograms = bad("not_calibrated", utcMs);
  uint8_t raw[3];
  if (readAt(bus_, address_, 0x00, raw, 3) != BusStatus::Ok) {
    out.status = DriverStatus::ReadFailed;
    out.kilograms = bad("read_failed", utcMs);
    return out;
  }
  out.hasRaw = true;
  out.raw = sign24(raw);
  out.rawSample = Sample::scalar(static_cast<double>(out.raw), utcMs);
  if (!tareSet_ || !spanSet_) {
    out.status = DriverStatus::NotConfigured;
    out.hasKilograms = false;
    return out;
  }
  const double kg =
      (static_cast<double>(out.raw) - static_cast<double>(tare_)) / span_;
  if (!finite(kg)) {
    out.status = DriverStatus::ReadFailed;
    out.hasKilograms = false;
    out.kilograms = bad("read_failed", utcMs);
    return out;
  }
  out.status = DriverStatus::Ok;
  out.hasKilograms = true;
  out.kilograms = Sample::scalar(kg, utcMs);
  return out;
}
DriverStatus Fdc1004::applyCapdac(uint8_t channel, uint8_t capdac) noexcept {
  if (channel > 3 || capdac > 31)
    return DriverStatus::InvalidArgument;
  if (applied_[channel]) {
    if (rejected_ != UINT32_MAX)
      ++rejected_;
    return DriverStatus::AlreadyApplied;
  }
  const uint8_t bytes[2] = {static_cast<uint8_t>(0x08u + channel), capdac};
  if (bus_.write(address_, bytes, 2) != BusStatus::Ok)
    return DriverStatus::ReadFailed;
  applied_[channel] = true;
  capdac_[channel] = capdac;
  return DriverStatus::Ok;
}
bool Fdc1004::capdacApplied(uint8_t channel) const noexcept {
  return channel < 4 && applied_[channel];
}
uint8_t Fdc1004::capdac(uint8_t channel) const noexcept {
  return channel < 4 && applied_[channel] ? capdac_[channel] : 0;
}
Fdc1004Reading Fdc1004::read(uint8_t channel, uint64_t utcMs) noexcept {
  Fdc1004Reading out;
  out.rawSample = bad("read_failed", utcMs);
  out.picofarads = bad("read_failed", utcMs);
  if (channel > 3) {
    out.status = DriverStatus::InvalidArgument;
    out.rawSample = bad("invalid_argument", utcMs);
    out.picofarads = bad("invalid_argument", utcMs);
    return out;
  }
  uint8_t raw[3];
  if (readAt(bus_, address_, static_cast<uint8_t>(channel * 3), raw, 3) !=
      BusStatus::Ok) {
    out.status = DriverStatus::ReadFailed;
    return out;
  }
  out.hasRaw = true;
  out.raw = sign24(raw);
  out.rawSample = Sample::scalar(static_cast<double>(out.raw), utcMs);
  out.capdacKnown = applied_[channel];
  out.capdac = applied_[channel] ? capdac_[channel] : 0;
  const double pf = static_cast<double>(out.raw) / 524288.0 +
                    static_cast<double>(out.capdac) * 3.125;
  if (!finite(pf)) {
    out.status = DriverStatus::ReadFailed;
    out.hasPicofarads = false;
    return out;
  }
  out.status = DriverStatus::Ok;
  out.hasPicofarads = true;
  out.picofarads = Sample::scalar(pf, utcMs);
  return out;
}
} // namespace cts
