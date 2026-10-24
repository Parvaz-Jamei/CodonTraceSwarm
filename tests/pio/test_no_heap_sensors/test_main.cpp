// SPDX-License-Identifier: MIT
// Unity suite [env:native_noheap]: zero heap calls in every src/cts/sensors
// adapter except FdcxCapSense (separate suite: both vendor libraries name a
// class FDC1004). Vendor APIs are the TEST ONLY fakes in tests/fakes.
#include <unity.h>
#include "heap_counter.h"
#include "fakes_impl.h"
#include "cts/sensors/Bme280Adafruit.h"
#include "cts/sensors/Hx711Bogde.h"
#include "cts/sensors/Ina219Adafruit.h"
#include "cts/sensors/Sht4xAdafruit.h"
#include "cts/sensors/Vl53l0xAdafruit.h"
#include "cts/sensors/Fdc1004Protocentral.h"
using namespace cts;
void setUp() {}
void tearDown() { heap::counting = false; }
static const uint64_t T = 1800000000000ULL;
static void test_sensor_adapters_allocate_nothing() {
  TwoWire wire; HX711 hx; Adafruit_INA219 ina; Adafruit_SHT4x sht; Adafruit_VL53L0X vl; FDC1004 fdc;
  Sample s[9]; Bme280Checked b;
  unsigned long n;
  {
    heap::Window w;
    for (int i = 0; i < 10; ++i) {
      b = bme280ReadChecked(wire, 0x76, T);
      s[0] = hx711Units(hx, T);
      s[1] = ina219ShuntMv(ina, T); s[2] = ina219BusV(ina, T); s[3] = ina219CurrentMa(ina, T);
      s[4] = sht4xTemperature(sht, T); s[5] = sht4xHumidity(sht, T);
      s[6] = vl53l0xMillimeters(vl, T);
      s[7] = protocentralFdc1004Picofarads(fdc, 1, T);
      s[8] = scalarIfFinite(1.0, T);
    }
    n = heap::allocs + heap::frees;
  }
  TEST_ASSERT_EQUAL_UINT32(0, n);
  TEST_ASSERT_TRUE(b.temperature.valid && b.pressure.valid && b.humidity.valid);
  for (int i = 0; i < 9; ++i) TEST_ASSERT_TRUE_MESSAGE(s[i].valid, "fake sensor read was not valid");
  TEST_ASSERT_TRUE(s[0].value == 12.5);
  TEST_ASSERT_TRUE(s[6].value == 123.0);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sensor_adapters_allocate_nothing);
  return UNITY_END();
}
