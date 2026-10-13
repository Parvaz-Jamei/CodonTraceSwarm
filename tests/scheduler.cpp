// SPDX-License-Identifier: MIT
// Synthetic scheduler attacks. Not a hardware sample clock.
#include "cts/SampleScheduler.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
static int checks = 0, failures = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      ++failures;                                                              \
      std::cerr << "FAIL " << __LINE__ << " " #x "\n";                         \
    }                                                                          \
  } while (0)
static cts::ChannelSpec spec(const char *name, bool rate, double lo, double hi) {
  cts::ChannelSpec c;
  c.name = name;
  c.quantity = "synthetic_signal";
  c.unit = "count";
  c.descriptor.featureId = "fixture";
  c.descriptor.configId = "fixture_v1";
  c.descriptor.sourceUnit = "count";
  c.descriptor.hasRate = rate;
  c.descriptor.minimumHz = lo;
  c.descriptor.maximumHz = hi;
  return c;
}
int main() {
  using namespace cts;
  Device device;
  auto primary = spec("s", true, 1, 10);
  auto other = spec("b", true, 1, 10);
  auto open = spec("free", false, 0, 0);
  CHECK(device.define(primary) == Error::Ok);
  CHECK(device.define(other) == Error::Ok);
  CHECK(device.define(open) == Error::Ok);
  primary.descriptor.maximumHz = 100; // Must not widen the stored registry.
  SampleScheduler scheduler(device);
  CHECK(scheduler.add("missing", 1, 0) == SchedulerCode::Rejected);
  CHECK(scheduler.add("free", 1, 0) == SchedulerCode::Rejected);
  CHECK(scheduler.add("s", 11, 0) == SchedulerCode::Rejected);
  CHECK(scheduler.add("s", 1, 0) == SchedulerCode::Ok);
  uint64_t deadline = 0;
  CHECK(scheduler.deadline("s", deadline) && deadline == 1000000);
  CHECK(scheduler.totalDrops() == 0);
  CHECK(scheduler.setRate("s", 11, 100) == SchedulerCode::Rejected);
  CHECK(scheduler.setRate("s", 0.5, 100) == SchedulerCode::Rejected);
  CHECK(scheduler.setRate("s", std::numeric_limits<double>::quiet_NaN(), 100) ==
        SchedulerCode::Rejected);
  CHECK(scheduler.deadline("s", deadline) && deadline == 1000000);
  CHECK(scheduler.totalDrops() == 0);
  CHECK(scheduler.setRate("s", 2, 100) == SchedulerCode::Ok);
  CHECK(scheduler.deadline("s", deadline) && deadline == 500100);
  CHECK(scheduler.totalDrops() == 0);
  CHECK(!scheduler.poll(100).due);
  auto one = scheduler.poll(500100);
  CHECK(one.due && one.missed == 0 && one.channel && !std::strcmp(one.channel, "s"));
  CHECK(!scheduler.poll(500100).due);
  CHECK(scheduler.setRate("s", 10, 0) == SchedulerCode::Ok);
  CHECK(scheduler.deadline("s", deadline) && deadline == 100000);
  const uint64_t burstAt = 100000ull + 100ull * 100000ull;
  auto burst = scheduler.poll(burstAt);
  CHECK(burst.due);
  CHECK(burst.missed == 100);
  CHECK(scheduler.totalDrops() == 100);
  CHECK(scheduler.drops("s") == 100);
  CHECK(!scheduler.poll(burstAt).due);
  CHECK(scheduler.totalDrops() == 100);
  Device secondDevice;
  CHECK(secondDevice.define(spec("a", true, 1, 10)) == Error::Ok);
  CHECK(secondDevice.define(spec("b", true, 1, 10)) == Error::Ok);
  SampleScheduler pair(secondDevice);
  CHECK(pair.add("a", 1, 0) == SchedulerCode::Ok);
  CHECK(pair.add("b", 1, 0) == SchedulerCode::Ok);
  auto first = pair.poll(1000000);
  auto second = pair.poll(1000000);
  CHECK(first.due && second.due && first.missed == 0 && second.missed == 0);
  CHECK(first.channel && second.channel && std::strcmp(first.channel, second.channel));
  CHECK(!pair.poll(1000000).due);
  CHECK(pair.totalDrops() == 0);
  {
    // UINT64_MAX is not representable as double and becomes 2^64, so
    // 1000000/hz is exactly 2^64. period() must return false before the cast.
    const double hz = 1000000.0 / static_cast<double>(UINT64_MAX);
    Device slow;
    CHECK(slow.define(spec("slow", true, hz, hz)) == Error::Ok);
    SampleScheduler wide(slow);
    uint64_t ignored = 1;
    CHECK(wide.add("slow", hz, 0) == SchedulerCode::Rejected);
    CHECK(!wide.deadline("slow", ignored));
    std::cout << "probe period_2_64=false\n";
  }
  if (failures)
    std::cerr << "checks=" << checks << " failures=" << failures << "\n";
  else
    std::cout << "PASS: scheduler set_rate/burst/bounds checks=" << checks
              << " failures=0\n";
  return failures ? 1 : 0;
}
