#include "cts/sensors/Bme280Comp.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
  const uint64_t utc = 42;
  // Fixed calibration image and raw ADC. dig_T1=27504 dig_T2=1000 dig_T3=-10
  // dig_P1=36477 dig_P2=-10630 dig_P3=3024 dig_P4=2855 dig_P5=140 dig_P6=-7
  // dig_P7=15500 dig_P8=-14600 dig_P9=6000 dig_H1=75 dig_H2=354 dig_H3=0
  // dig_H4=261 dig_H5=50 dig_H6=30. adc_T=519888 adc_P=415148 adc_H=30000.
  const uint8_t tempPress[26] = {
      0x70, 0x6B, 0xE8, 0x03, 0xF6, 0xFF, 0x7D, 0x8E, 0x7A, 0xD6, 0xD0, 0x0B, 0x27,
      0x0B, 0x8C, 0x00, 0xF9, 0xFF, 0x8C, 0x3C, 0xF8, 0xC6, 0x70, 0x17, 0x00, 0x4B};
  const uint8_t humidity[7] = {0x62, 0x01, 0x00, 0x10, 0x25, 0x03, 0x1E};
  const uint8_t measurement[8] = {0x65, 0x5A, 0xC0, 0x7E, 0xED, 0x00, 0x75, 0x30};
  cts::Bme280Calib cal;
  cts::Bme280Adc raw;
  assert(cts::bme280UnpackCalibration(tempPress, 26, humidity, 7, cal));
  assert(cts::bme280UnpackMeasurement(measurement, 8, raw));
  assert(cal.dig_T1 == 27504);
  assert(cal.dig_T2 == 1000);
  assert(cal.dig_T3 == -10);
  assert(cal.dig_P1 == 36477);
  assert(cal.dig_P2 == -10630);
  assert(cal.dig_P9 == 6000);
  assert(cal.dig_H1 == 75);
  assert(cal.dig_H2 == 354);
  assert(cal.dig_H4 == 261);
  assert(cal.dig_H5 == 50);
  assert(cal.dig_H6 == 30);
  assert(raw.adc_T == 519888);
  assert(raw.adc_P == 415148);
  assert(raw.adc_H == 30000);
  const cts::Bme280Compensated comp = cts::bme280Compensate(cal, raw);
  // Same integer formulas: t_fine=4869, T=(t_fine*5+128)/256=95,
  // pressure var4=24827812, humidity count H=71625.
  const double expectTemperature = (double)((float)95 / 100);
  const double expectPressure = (double)((float)(24827812 / 256.0));
  const double expectHumidity = (double)((float)((float)71625 / 1024.0));
  assert(comp.t_fine == 4869);
  assert(comp.temperatureC == expectTemperature);
  assert(comp.pressurePa == expectPressure);
  assert(comp.humidityPercent == expectHumidity);
  const cts::Bme280Samples full = cts::bme280SamplesFromRegisters(
      tempPress, 26, humidity, 7, measurement, 8, utc);
  assert(full.temperature.valid);
  assert(full.pressure.valid);
  assert(full.humidity.valid);
  assert(full.temperature.value == expectTemperature);
  assert(full.pressure.value == expectPressure);
  assert(full.humidity.value == expectHumidity);
  assert(full.temperature.utcMs == utc);
  assert(std::strcmp(full.pressure.status, "ok") == 0);

  // Division by zero in the Adafruit pressure formula is numeric 0, and that
  // 0 is a valid sample only when the transfer length is complete.
  cts::Bme280Calib zeroDiv = cal;
  zeroDiv.dig_P1 = 0;
  const cts::Bme280Compensated zeroed = cts::bme280Compensate(zeroDiv, raw);
  assert(zeroed.pressurePa == 0.0);
  const cts::Sample keptZero =
      cts::bme280ShortReadSample(8, 8, zeroed.pressurePa, utc);
  assert(keptZero.valid);
  assert(keptZero.value == 0.0);
  assert(std::strcmp(keptZero.status, "ok") == 0);

  const double candidate = expectPressure;
  const cts::Sample shortRead = cts::bme280ShortReadSample(3, 8, candidate, utc);
  assert(shortRead.valid == false);
  assert(std::strcmp(shortRead.status, "read_failed") == 0);
  assert(shortRead.utcMs == utc);
  assert(shortRead.value != candidate);
  assert(!(shortRead.valid && shortRead.value == candidate));
  const uint8_t shortMeasurement[4] = {0x65, 0x5A, 0xC0, 0x7E};
  const cts::Bme280Samples missed = cts::bme280SamplesFromRegisters(
      tempPress, 26, humidity, 7, shortMeasurement, 4, utc);
  assert(missed.temperature.valid == false);
  assert(missed.pressure.valid == false);
  assert(missed.humidity.valid == false);
  assert(std::strcmp(missed.pressure.status, "read_failed") == 0);
  assert(missed.pressure.value != expectPressure);
  assert(!(missed.pressure.valid));
  std::cout << "PASS: bme280 compensation temperatureC=" << expectTemperature
            << " pressurePa=" << expectPressure
            << " humidityPercent=" << expectHumidity
            << "; short read invalid and not a valid value\n";
}
