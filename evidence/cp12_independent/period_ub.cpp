#include "cts/SampleScheduler.h"
#include <cstdint>
#include <iostream>
#include <limits>
static cts::ChannelSpec spec() {
  cts::ChannelSpec c;
  c.name = "slow";
  c.quantity = "synthetic_signal";
  c.unit = "count";
  c.descriptor.featureId = "fixture";
  c.descriptor.configId = "fixture_v1";
  c.descriptor.sourceUnit = "count";
  c.descriptor.hasRate = true;
  const double hz =
      1000000.0 / static_cast<double>(std::numeric_limits<uint64_t>::max());
  c.descriptor.minimumHz = hz;
  c.descriptor.maximumHz = hz;
  return c;
}
int main() {
  cts::Device device;
  const auto defined = device.define(spec());
  std::cout << "define=" << static_cast<int>(defined) << "\n";
  if (defined != cts::Error::Ok)
    return 0;
  cts::SampleScheduler scheduler(device);
  const double hz =
      1000000.0 / static_cast<double>(std::numeric_limits<uint64_t>::max());
  const auto code = scheduler.add("slow", hz, 0);
  std::cout << "add=" << static_cast<int>(code) << "\n";
  return 0;
}
