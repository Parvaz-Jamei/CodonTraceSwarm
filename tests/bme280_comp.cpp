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
  // Regression of the Adafruit BME280 Library 2.3.0 integer sequence
  // (Adafruit_BME280.cpp, commit ef712e95147de926b4545e379f6296ad271529b5).
  // Not a datasheet certificate. t_fine=4869, T=(t_fine*5+128)/256=95,
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

  // dig_P1==0 is not a pressure measurement. The division-guard 0 is not
  // published as a valid sample. Temperature and humidity still run.
  uint8_t zeroP1[26];
  std::memcpy(zeroP1, tempPress, sizeof zeroP1);
  zeroP1[6] = 0;
  zeroP1[7] = 0;
  cts::Bme280Calib zeroCal;
  assert(cts::bme280UnpackCalibration(zeroP1, 26, humidity, 7, zeroCal));
  assert(zeroCal.dig_P1 == 0);
  const cts::Bme280Samples badP1 = cts::bme280SamplesFromRegisters(
      zeroP1, 26, humidity, 7, measurement, 8, utc);
  assert(badP1.pressure.valid == false);
  assert(std::strcmp(badP1.pressure.status, "invalid_calibration") == 0);
  assert(badP1.pressure.utcMs == utc);
  assert(!(badP1.pressure.valid && badP1.pressure.value == 0.0));
  assert(badP1.temperature.valid);
  assert(badP1.temperature.value == expectTemperature);
  assert(badP1.humidity.valid);
  assert(badP1.humidity.value == expectHumidity);

  // All-zero calibration has dig_P1==0, so pressure is invalid and the
  // pressure division is not run. A numeric zero temperature stays valid.
  const uint8_t zeros26[26] = {};
  const uint8_t zeros7[7] = {};
  const uint8_t zeros8[8] = {};
  const cts::Bme280Samples allZero = cts::bme280SamplesFromRegisters(
      zeros26, 26, zeros7, 7, zeros8, 8, utc);
  assert(allZero.pressure.valid == false);
  assert(std::strcmp(allZero.pressure.status, "invalid_calibration") == 0);
  assert(allZero.temperature.valid);
  assert(allZero.temperature.value == 0.0);
  assert(std::strcmp(allZero.temperature.status, "ok") == 0);

  // A finite 0 from a complete transfer is still a valid sample. This does
  // not make dig_P1==0 a pressure reading.
  const cts::Sample genuineZero = cts::bme280ShortReadSample(8, 8, 0.0, utc);
  assert(genuineZero.valid);
  assert(genuineZero.value == 0.0);
  assert(std::strcmp(genuineZero.status, "ok") == 0);

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

  // Missing calibration must not turn a full measurement into pressure 0.
  const cts::Bme280Samples missing = cts::bme280SamplesFromRegisters(
      nullptr, 0, humidity, 7, measurement, 8, utc);
  assert(missing.temperature.valid == false);
  assert(missing.pressure.valid == false);
  assert(missing.humidity.valid == false);
  assert(std::strcmp(missing.temperature.status, "read_failed") == 0);
  assert(std::strcmp(missing.pressure.status, "read_failed") == 0);
  assert(std::strcmp(missing.humidity.status, "read_failed") == 0);
  assert(missing.pressure.utcMs == utc);
  assert(missing.pressure.value != expectPressure);
  assert(!(missing.pressure.valid));

  // humidity[3]=0xff, humidity[4]=0x00. 12-bit field 0xFF0 sign-extends to -16.
  // humidity[5]=0xff exercises the same decode for dig_H5.
  uint8_t humidNeg[7] = {0x62, 0x01, 0x00, 0xff, 0x00, 0xff, 0x1E};
  cts::Bme280Calib neg;
  assert(cts::bme280UnpackCalibration(tempPress, 26, humidNeg, 7, neg));
  assert(neg.dig_H4 == -16);
  assert(neg.dig_H5 == -16);

  std::cout << "PASS: bme280 compensation temperatureC=" << expectTemperature
            << " pressurePa=" << expectPressure
            << " humidityPercent=" << expectHumidity
            << "; short read and missing calibration invalid;"
            << " dig_P1==0 invalid_calibration; dig_H4=-16\n";
}
