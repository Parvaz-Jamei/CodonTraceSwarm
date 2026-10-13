// SPDX-License-Identifier: MIT
// Synthetic mock-driver attacks. Numbers are not bench measurements.
#include "cts/MockDrivers.h"
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
struct Mem : cts::ByteBus {
  uint8_t mem[256] = {};
  bool failWrite = false, failRead = false;
  int writes = 0, reads = 0;
  uint8_t pointer = 0;
  cts::BusStatus write(uint8_t, const uint8_t *data, size_t n) noexcept override {
    ++writes;
    if (failWrite || !data || !n)
      return cts::BusStatus::Fail;
    pointer = data[0];
    for (size_t i = 1; i < n; ++i)
      mem[static_cast<uint8_t>(pointer + i - 1)] = data[i];
    return cts::BusStatus::Ok;
  }
  cts::BusStatus read(uint8_t, uint8_t *data, size_t n) noexcept override {
    ++reads;
    if (failRead || !data || !n)
      return cts::BusStatus::Fail;
    for (size_t i = 0; i < n; ++i)
      data[i] = mem[static_cast<uint8_t>(pointer + i)];
    return cts::BusStatus::Ok;
  }
};
static void putLe(Mem &bus, uint8_t reg, int32_t v) {
  const uint32_t u = static_cast<uint32_t>(v);
  bus.mem[reg] = static_cast<uint8_t>(u);
  bus.mem[reg + 1] = static_cast<uint8_t>(u >> 8);
  bus.mem[reg + 2] = static_cast<uint8_t>(u >> 16);
  bus.mem[reg + 3] = static_cast<uint8_t>(u >> 24);
}
static bool encodesInvalid(cts::Device &device, const char *channel,
                           const cts::Sample &sample) {
  char out[cts::TxBytes];
  if (!std::isfinite(sample.value) || sample.valid)
    return false;
  if (std::strcmp(sample.status, "ok") == 0)
    return false;
  if (device.reading(channel, sample, out, sizeof(out)) != cts::Error::Ok)
    return false;
  return std::strstr(out, "\"valid\":false") && std::strstr(out, "\"value\":null") &&
         !std::strstr(out, "\"value\":0");
}
static cts::ChannelSpec channel(const char *name) {
  cts::ChannelSpec c;
  c.name = name;
  c.quantity = "synthetic_signal";
  c.unit = "count";
  c.descriptor.featureId = "fixture";
  c.descriptor.configId = "fixture_v1";
  c.descriptor.sourceUnit = "count";
  return c;
}
int main() {
  using namespace cts;
  const uint64_t utc = 1800000000000ULL;
  Device device;
  const char *names[] = {"t", "p", "h", "d", "raw", "pf", "shunt"};
  for (auto name : names)
    CHECK(device.define(channel(name)) == Error::Ok);
  SessionConfig session;
  session.swarmId = "lab";
  session.nodeId = "node";
  session.bootId = "boot";
  CHECK(device.begin(session) == Error::Ok);
  {
    Mem bus;
    bus.mem[0xD0] = 0x60;
    bus.mem[0xF3] = 0x01;
    putLe(bus, 0x10, 0);
    putLe(bus, 0x14, 0);
    putLe(bus, 0x18, 0);
    Bme280 sensor(bus, 0x76);
    auto reading = sensor.read(utc);
    CHECK(reading.status == DriverStatus::NotReady);
    CHECK(encodesInvalid(device, "t", reading.temperature));
    CHECK(encodesInvalid(device, "p", reading.pressure));
    CHECK(encodesInvalid(device, "h", reading.humidity));
    bus.mem[0xF3] = 0;
    bus.failRead = true;
    reading = sensor.read(utc);
    CHECK(reading.status == DriverStatus::ReadFailed);
    CHECK(encodesInvalid(device, "t", reading.temperature));
    bus.failRead = false;
    putLe(bus, 0x10, 0);
    putLe(bus, 0x14, 101325);
    putLe(bus, 0x18, 50000);
    reading = sensor.read(utc);
    CHECK(reading.status == DriverStatus::Ok);
    CHECK(reading.temperature.valid && reading.temperature.value == 0);
    char out[TxBytes];
    CHECK(device.reading("t", reading.temperature, out, sizeof(out)) == Error::Ok);
    CHECK(std::strstr(out, "\"valid\":true") && std::strstr(out, "\"value\":0"));
    CHECK(reading.pressure.valid && reading.pressure.value == 101325);
    CHECK(reading.humidity.valid && reading.humidity.value == 50);
  }
  {
    Mem bus;
    Sht4x sensor(bus, 0x44);
    Sht4xView view;
    CHECK(Sht4x::bind("pressure", view) == DriverStatus::Unsupported);
    CHECK(!view.bound);
    const int writes = bus.writes;
    Sample attacked = Sample::scalar(0, utc);
    CHECK(sensor.readQuantity("pressure", attacked, utc) ==
          DriverStatus::Unsupported);
    CHECK(bus.writes == writes);
    CHECK(encodesInvalid(device, "p", attacked));
    CHECK(Sht4x::bind("temperature", view) == DriverStatus::Ok);
    CHECK(view.bound);
    bus.failRead = true;
    auto failed = sensor.read(utc);
    CHECK(failed.status == DriverStatus::ReadFailed);
    CHECK(encodesInvalid(device, "t", failed.temperature));
    CHECK(encodesInvalid(device, "h", failed.humidity));
    bus.failRead = false;
    bus.mem[0xFD] = 0x80;
    bus.mem[0xFE] = 0x00;
    bus.mem[static_cast<uint8_t>(0xFD + 3)] = 0x80;
    bus.mem[static_cast<uint8_t>(0xFD + 4)] = 0x00;
    auto ok = sensor.read(utc);
    CHECK(ok.status == DriverStatus::Ok);
    CHECK(ok.temperature.valid && ok.humidity.valid);
    CHECK(std::isfinite(ok.temperature.value) && std::isfinite(ok.humidity.value));
    Sample only;
    CHECK(sensor.readQuantity("humidity", only, utc) == DriverStatus::Ok);
    CHECK(only.valid);
  }
  {
    Mem bus;
    bus.mem[0x10] = 0x03;
    bus.mem[0x11] = 0xE8;
    bus.mem[0x12] = 0xFF;
    bus.mem[0x13] = 0xFE;
    Ina219 sensor(bus, 0x40);
    auto raw = sensor.read(utc);
    CHECK(raw.hasCounts);
    CHECK(raw.shuntCounts == 1000);
    CHECK(raw.busCounts == -2);
    CHECK(!raw.hasCurrent);
    CHECK(raw.current.valid == false);
    CHECK(encodesInvalid(device, "shunt", raw.current));
    CHECK(sensor.setShuntOhms(0) == DriverStatus::InvalidArgument);
    CHECK(sensor.setShuntOhms(0.1) == DriverStatus::Ok);
    auto scaled = sensor.read(utc);
    CHECK(scaled.hasCurrent);
    CHECK(scaled.status == DriverStatus::Ok);
    CHECK(std::fabs(scaled.current.value - 0.1) < 1e-12);
    bus.failRead = true;
    auto bad = sensor.read(utc);
    CHECK(!bad.hasCurrent && !bad.hasCounts);
    CHECK(encodesInvalid(device, "shunt", bad.shunt));
  }
  {
    Mem bus;
    bus.mem[0x00] = 2;
    bus.mem[0x02] = 0;
    bus.mem[0x03] = 0;
    Vl53l0x sensor(bus, 0x29);
    auto timeout = sensor.read(utc);
    CHECK(!timeout.rangeValid);
    CHECK(std::strcmp(timeout.distance.status, "timeout") == 0);
    CHECK(encodesInvalid(device, "d", timeout.distance));
    bus.mem[0x00] = 1;
    auto wait = sensor.read(utc);
    CHECK(!wait.rangeValid);
    CHECK(std::strcmp(wait.distance.status, "not_ready") == 0);
    CHECK(encodesInvalid(device, "d", wait.distance));
    bus.mem[0x00] = 0;
    auto zero = sensor.read(utc);
    CHECK(zero.rangeValid && zero.distance.valid && zero.distance.value == 0);
    char out[TxBytes];
    CHECK(device.reading("d", zero.distance, out, sizeof(out)) == Error::Ok);
    CHECK(std::strstr(out, "\"valid\":true") && std::strstr(out, "\"value\":0"));
  }
  {
    Mem bus;
    bus.mem[0] = 0x80;
    bus.mem[1] = 0x00;
    bus.mem[2] = 0x00;
    Hx711 sensor(bus, 0x00);
    auto edge = sensor.read(utc);
    CHECK(edge.hasRaw && edge.raw == -8388608);
    CHECK(edge.rawSample.valid && edge.rawSample.value == -8388608);
    CHECK(!edge.hasKilograms);
    CHECK(encodesInvalid(device, "raw", edge.kilograms));
    CHECK(sensor.setSpanCountsPerKilogram(0) == DriverStatus::InvalidArgument);
    CHECK(sensor.setTare(0) == DriverStatus::Ok);
    CHECK(!sensor.calibrated());
    CHECK(sensor.setSpanCountsPerKilogram(1000) == DriverStatus::Ok);
    bus.mem[0] = 0x00;
    bus.mem[1] = 0x03;
    bus.mem[2] = 0xE8;
    auto kg = sensor.read(utc);
    CHECK(kg.hasKilograms && kg.raw == 1000 && kg.kilograms.value == 1);
    bus.failRead = true;
    auto bad = sensor.read(utc);
    CHECK(!bad.hasRaw && !bad.hasKilograms);
    CHECK(encodesInvalid(device, "raw", bad.rawSample));
  }
  {
    Mem bus;
    Fdc1004 sensor(bus, 0x50);
    CHECK(sensor.applyCapdac(0, 5) == DriverStatus::Ok);
    const int writes = bus.writes;
    CHECK(sensor.applyCapdac(0, 9) == DriverStatus::AlreadyApplied);
    CHECK(sensor.rejectedCapdacApplies() == 1);
    CHECK(sensor.capdac(0) == 5);
    CHECK(bus.writes == writes);
    CHECK(sensor.applyCapdac(0, 5) == DriverStatus::AlreadyApplied);
    CHECK(sensor.rejectedCapdacApplies() == 2);
    bus.mem[0] = 0x00;
    bus.mem[1] = 0x00;
    bus.mem[2] = 0x00;
    bus.failRead = true;
    auto failed = sensor.read(0, utc);
    CHECK(failed.status == DriverStatus::ReadFailed);
    CHECK(!failed.hasPicofarads && !failed.hasRaw);
    CHECK(encodesInvalid(device, "pf", failed.picofarads));
    bus.failRead = false;
    bus.mem[0] = 0x80;
    bus.mem[1] = 0x00;
    bus.mem[2] = 0x00;
    auto ok = sensor.read(0, utc);
    CHECK(ok.hasRaw && ok.raw == -8388608);
    CHECK(ok.capdacKnown && ok.capdac == 5);
    CHECK(ok.hasPicofarads && ok.picofarads.valid);
    CHECK(ok.picofarads.value != 0);
  }
  if (failures)
    std::cerr << "checks=" << checks << " failures=" << failures << "\n";
  else
    std::cout << "PASS: mock drivers synthetic attacks checks=" << checks
              << " failures=0\n";
  return failures ? 1 : 0;
}
