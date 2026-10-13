#include "cts/SensorSample.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
int main() {
  const uint64_t utc = 42;
  const cts::Sample zero = cts::scalarIfFinite(0.0, utc);
  assert(zero.valid);
  assert(zero.value == 0.0);
  assert(zero.utcMs == utc);
  const cts::Sample negative = cts::scalarIfFinite(-12.5, utc);
  assert(negative.valid);
  assert(negative.value == -12.5);
  assert(negative.utcMs == utc);
  const double bad[] = {std::numeric_limits<double>::quiet_NaN(),
                        std::numeric_limits<double>::infinity(),
                        -std::numeric_limits<double>::infinity()};
  for (size_t i = 0; i < 3; ++i) {
    const cts::Sample sample = cts::scalarIfFinite(bad[i], utc);
    assert(sample.valid == false);
    assert(std::strcmp(sample.status, "read_failed") == 0);
    assert(sample.utcMs == utc);
  }
  std::cout << "PASS: scalarIfFinite keeps 0 and negatives valid; NaN and infinities are read_failed\n";
}
