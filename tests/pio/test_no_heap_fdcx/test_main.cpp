// SPDX-License-Identifier: MIT
// Unity suite [env:native_noheap]: zero heap calls in src/cts/sensors/FdcxCapSense.h.
#include <unity.h>
#define FAKE_FDCX
#include "heap_counter.h"
#include "fakes_impl.h"
#include "cts/sensors/FdcxCapSense.h"
using namespace cts;
void setUp() {}
void tearDown() { heap::counting = false; }
static void test_fdcx_adapters_allocate_nothing() {
  FDC1004 cap; FDC2x1x hz; Sample a, b, c;
  unsigned long n;
  {
    heap::Window w;
    for (int i = 0; i < 10; ++i) {
      a = fdcxFdc1004Picofarads(cap, FDC1004Measurement::M1, 1800000000000ULL);
      b = fdcxFdc2214Hertz(hz, FDC2x1xChannel::Ch0, 40000000.0f, 1800000000000ULL);
      c = fdcxFdc2214Hertz(hz, FDC2x1xChannel::Ch0, 0.0f, 1800000000000ULL);
    }
    n = heap::allocs + heap::frees;
  }
  TEST_ASSERT_EQUAL_UINT32(0, n);
  TEST_ASSERT_TRUE(a.valid);
  TEST_ASSERT_TRUE(a.value == 4.25);
  TEST_ASSERT_TRUE(b.valid);
  TEST_ASSERT_FALSE(c.valid);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fdcx_adapters_allocate_nothing);
  return UNITY_END();
}
