// SPDX-License-Identifier: MIT
// TEST ONLY: deterministic, non-allocating definitions of the fake vendor APIs.
// Include in exactly one TU per test binary. FAKE_FDCX selects the FDCxCapSense
// fakes; otherwise the ProtoCentral FDC1004 fake is defined (both name FDC1004).
#pragma once
#include "Wire.h"
#include "HX711.h"
#include "Adafruit_INA219.h"
#include "Adafruit_SHT4x.h"
#include "Adafruit_VL53L0X.h"
#ifdef FAKE_FDCX
#include "FDCxCapSense_FDC2x1x.h"
FDCxStatus FDC1004::readCapacitancePf(FDC1004Measurement, float &pf) { pf = 4.25f; return FDCxStatus::Ok; }
FDCxStatus FDC2x1x::readFrequencyHzFromRegisters(FDC2x1xChannel, float fclk, FDC2x1xFrequency &o) {
  o.status = FDCxStatus::Ok; o.frequencyHz = fclk / 8; return FDCxStatus::Ok;
}
#else
#include "Protocentral_FDC1004.h"
float FDC1004::getCapacitancePicofarads(fdc1004_channel_t c) { return 1.5f + c; }
#endif
// BME280 register image: 26 calibration bytes at 0x88, 7 at 0xE1, 8 data at 0xF7.
static uint8_t fakeWireReg = 0;
static size_t fakeWireLeft = 0, fakeWirePos = 0;
void TwoWire::beginTransmission(uint8_t) {}
size_t TwoWire::write(uint8_t r) { fakeWireReg = r; return 1; }
uint8_t TwoWire::endTransmission() { return 0; }
size_t TwoWire::requestFrom(uint8_t, size_t n) { fakeWireLeft = n; fakeWirePos = 0; return n; }
int TwoWire::available() { return (int)fakeWireLeft; }
int TwoWire::read() {
  if (!fakeWireLeft) return -1;
  --fakeWireLeft;
  const size_t i = fakeWirePos++;
  if (fakeWireReg == 0x88) { // same register image as tests/bme280_comp.cpp
    static const uint8_t cal[26] = {0x70,0x6B,0xE8,0x03,0xF6,0xFF,0x7D,0x8E,0x7A,0xD6,0xD0,0x0B,0x27,
                                    0x0B,0x8C,0x00,0xF9,0xFF,0x8C,0x3C,0xF8,0xC6,0x70,0x17,0x00,0x4B};
    return cal[i];
  }
  if (fakeWireReg == 0xE1) { static const uint8_t h[7] = {0x62,0x01,0x00,0x10,0x25,0x03,0x1E}; return h[i]; }
  static const uint8_t m[8] = {0x65,0x5A,0xC0,0x7E,0xED,0x00,0x75,0x30};
  return m[i];
}
bool HX711::is_ready() { return true; }
float HX711::get_scale() { return 2.0f; }
float HX711::get_units(uint8_t) { return 12.5f; }
float Adafruit_INA219::getShuntVoltage_mV() { return 0.5f; }
float Adafruit_INA219::getBusVoltage_V() { return 3.3f; }
float Adafruit_INA219::getCurrent_mA() { return 0.0f; }
bool Adafruit_INA219::success() { return true; }
bool Adafruit_SHT4x::getEvent(sensors_event_t *h, sensors_event_t *t) {
  h->relative_humidity = 41.0f; t->temperature = 22.5f; return true;
}
VL53L0X_Error Adafruit_VL53L0X::rangingTest(VL53L0X_RangingMeasurementData_t *d, bool) {
  d->RangeMilliMeter = 123; d->RangeStatus = 0; return VL53L0X_ERROR_NONE;
}
