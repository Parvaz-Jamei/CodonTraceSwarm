// SPDX-License-Identifier: MIT
// Symbol-check TU (tools/check_no_heap.py): emits every header-only sensor
// adapter except FdcxCapSense against declaration-only vendor fakes.
#include "cts/sensors/Bme280Adafruit.h"
#include "cts/sensors/Hx711Bogde.h"
#include "cts/sensors/Ina219Adafruit.h"
#include "cts/sensors/Sht4xAdafruit.h"
#include "cts/sensors/Vl53l0xAdafruit.h"
#include "cts/sensors/Fdc1004Protocentral.h"
namespace emit {
cts::Bme280Checked bme(TwoWire &w, uint64_t t) { return cts::bme280ReadChecked(w, 0x76, t); }
cts::Sample hx(HX711 &c, uint64_t t) { return cts::hx711Units(c, t); }
cts::Sample inaS(Adafruit_INA219 &c, uint64_t t) { return cts::ina219ShuntMv(c, t); }
cts::Sample inaB(Adafruit_INA219 &c, uint64_t t) { return cts::ina219BusV(c, t); }
cts::Sample inaC(Adafruit_INA219 &c, uint64_t t) { return cts::ina219CurrentMa(c, t); }
cts::Sample shtT(Adafruit_SHT4x &c, uint64_t t) { return cts::sht4xTemperature(c, t); }
cts::Sample shtH(Adafruit_SHT4x &c, uint64_t t) { return cts::sht4xHumidity(c, t); }
cts::Sample vl(Adafruit_VL53L0X &c, uint64_t t) { return cts::vl53l0xMillimeters(c, t); }
cts::Sample pc(FDC1004 &c, uint64_t t) { return cts::protocentralFdc1004Picofarads(c, 1, t); }
cts::Sample fin(double v, uint64_t t) { return cts::scalarIfFinite(v, t); }
} // namespace emit
