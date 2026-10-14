// SPDX-License-Identifier: MIT
#pragma once
// Pure BME280 compensation. No Arduino, Wire, or Adafruit headers.
// Integer steps are copied from Adafruit_BME280.cpp readTemperature,
// readPressure, and readHumidity (library 2.3.0). t_fine is produced from
// the same temperature bytes that feed pressure and humidity.
// Sampling registers inside Adafruit_BME280 are not visible here. A missing
// calibration image is not a number. Pressure's var1==0 case is numeric 0
// only for a transfer that already succeeded.
#include "../SensorSample.h"
#include <stddef.h>
#include <stdint.h>
namespace cts {
struct Bme280Calib {
  uint16_t dig_T1;
  int16_t dig_T2;
  int16_t dig_T3;
  uint16_t dig_P1;
  int16_t dig_P2;
  int16_t dig_P3;
  int16_t dig_P4;
  int16_t dig_P5;
  int16_t dig_P6;
  int16_t dig_P7;
  int16_t dig_P8;
  int16_t dig_P9;
  uint8_t dig_H1;
  int16_t dig_H2;
  uint8_t dig_H3;
  int16_t dig_H4;
  int16_t dig_H5;
  int8_t dig_H6;
};
struct Bme280Adc {
  int32_t adc_T;
  int32_t adc_P;
  int32_t adc_H;
};
struct Bme280Compensated {
  int32_t t_fine;
  double temperatureC;
  double pressurePa;
  double humidityPercent;
};
struct Bme280Samples {
  Sample temperature;
  Sample pressure;
  Sample humidity;
};
// bytesGot < bytesNeeded is a short read. candidate is not published:
// a failed transfer is never a valid number, including the pressure formula's 0.
inline Sample bme280ShortReadSample(size_t bytesGot, size_t bytesNeeded,
                                    double candidate, uint64_t utcMs) {
  if (bytesGot < bytesNeeded)
    return Sample::invalid("read_failed", utcMs);
  (void)candidate;
  return scalarIfFinite(candidate, utcMs);
}
inline uint16_t bme280LoadLe16(const uint8_t *p) {
  return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) |
                               (static_cast<uint16_t>(p[1]) << 8));
}
inline bool bme280UnpackCalibration(const uint8_t *tempPress, size_t tempPressLen,
                                    const uint8_t *humidity, size_t humidityLen,
                                    Bme280Calib &cal) {
  if (tempPress == nullptr || humidity == nullptr || tempPressLen < 26 ||
      humidityLen < 7)
    return false;
  cal.dig_T1 = bme280LoadLe16(tempPress + 0);
  cal.dig_T2 = static_cast<int16_t>(bme280LoadLe16(tempPress + 2));
  cal.dig_T3 = static_cast<int16_t>(bme280LoadLe16(tempPress + 4));
  cal.dig_P1 = bme280LoadLe16(tempPress + 6);
  cal.dig_P2 = static_cast<int16_t>(bme280LoadLe16(tempPress + 8));
  cal.dig_P3 = static_cast<int16_t>(bme280LoadLe16(tempPress + 10));
  cal.dig_P4 = static_cast<int16_t>(bme280LoadLe16(tempPress + 12));
  cal.dig_P5 = static_cast<int16_t>(bme280LoadLe16(tempPress + 14));
  cal.dig_P6 = static_cast<int16_t>(bme280LoadLe16(tempPress + 16));
  cal.dig_P7 = static_cast<int16_t>(bme280LoadLe16(tempPress + 18));
  cal.dig_P8 = static_cast<int16_t>(bme280LoadLe16(tempPress + 20));
  cal.dig_P9 = static_cast<int16_t>(bme280LoadLe16(tempPress + 22));
  cal.dig_H1 = tempPress[25];
  cal.dig_H2 = static_cast<int16_t>(bme280LoadLe16(humidity + 0));
  cal.dig_H3 = humidity[2];
  // Adafruit readCoefficients: ((int8_t)reg << 4) combined with the shared nibble.
  cal.dig_H4 = static_cast<int16_t>((static_cast<int8_t>(humidity[3]) << 4) |
                                   (humidity[4] & 0x0F));
  cal.dig_H5 = static_cast<int16_t>((static_cast<int8_t>(humidity[5]) << 4) |
                                   (humidity[4] >> 4));
  cal.dig_H6 = static_cast<int8_t>(humidity[6]);
  return true;
}
inline bool bme280UnpackMeasurement(const uint8_t *data, size_t len, Bme280Adc &adc) {
  if (data == nullptr || len < 8)
    return false;
  const uint32_t rawP = (static_cast<uint32_t>(data[0]) << 16) |
                        (static_cast<uint32_t>(data[1]) << 8) |
                        static_cast<uint32_t>(data[2]);
  const uint32_t rawT = (static_cast<uint32_t>(data[3]) << 16) |
                        (static_cast<uint32_t>(data[4]) << 8) |
                        static_cast<uint32_t>(data[5]);
  adc.adc_P = static_cast<int32_t>(rawP >> 4);
  adc.adc_T = static_cast<int32_t>(rawT >> 4);
  adc.adc_H = static_cast<int32_t>((static_cast<uint32_t>(data[6]) << 8) |
                                  static_cast<uint32_t>(data[7]));
  return true;
}
inline Bme280Compensated bme280Compensate(const Bme280Calib &cal, const Bme280Adc &adc) {
  Bme280Compensated out;
  int32_t var1, var2;
  const int32_t adc_T = adc.adc_T;
  var1 = (int32_t)((adc_T / 8) - ((int32_t)cal.dig_T1 * 2));
  var1 = (var1 * ((int32_t)cal.dig_T2)) / 2048;
  var2 = (int32_t)((adc_T / 16) - ((int32_t)cal.dig_T1));
  var2 = (((var2 * var2) / 4096) * ((int32_t)cal.dig_T3)) / 16384;
  const int32_t t_fine_adjust = 0;
  out.t_fine = var1 + var2 + t_fine_adjust;
  const int32_t T = (out.t_fine * 5 + 128) / 256;
  const float temperature = (float)T / 100;
  out.temperatureC = (double)temperature;

  int64_t p1, p2, p3, p4;
  const int32_t adc_P = adc.adc_P;
  p1 = ((int64_t)out.t_fine) - 128000;
  p2 = p1 * p1 * (int64_t)cal.dig_P6;
  p2 = p2 + ((p1 * (int64_t)cal.dig_P5) * 131072);
  p2 = p2 + (((int64_t)cal.dig_P4) * 34359738368);
  p1 = ((p1 * p1 * (int64_t)cal.dig_P3) / 256) +
       ((p1 * ((int64_t)cal.dig_P2) * 4096));
  p3 = ((int64_t)1) * 140737488355328;
  p1 = (p3 + p1) * ((int64_t)cal.dig_P1) / 8589934592;
  if (p1 == 0) {
    out.pressurePa = 0;
  } else {
    p4 = 1048576 - adc_P;
    p4 = (((p4 * 2147483648) - p2) * 3125) / p1;
    p1 = (((int64_t)cal.dig_P9) * (p4 / 8192) * (p4 / 8192)) / 33554432;
    p2 = (((int64_t)cal.dig_P8) * p4) / 524288;
    p4 = ((p4 + p1 + p2) / 256) + (((int64_t)cal.dig_P7) * 16);
    const float pressure = (float)(p4 / 256.0);
    out.pressurePa = (double)pressure;
  }

  int32_t h1, h2, h3, h4, h5;
  const int32_t adc_H = adc.adc_H;
  h1 = out.t_fine - ((int32_t)76800);
  h2 = (int32_t)(adc_H * 16384);
  h3 = (int32_t)(((int32_t)cal.dig_H4) * 1048576);
  h4 = ((int32_t)cal.dig_H5) * h1;
  h5 = (((h2 - h3) - h4) + (int32_t)16384) / 32768;
  h2 = (h1 * ((int32_t)cal.dig_H6)) / 1024;
  h3 = (h1 * ((int32_t)cal.dig_H3)) / 2048;
  h4 = ((h2 * (h3 + (int32_t)32768)) / 1024) + (int32_t)2097152;
  h2 = ((h4 * ((int32_t)cal.dig_H2)) + 8192) / 16384;
  h3 = h5 * h2;
  h4 = ((h3 / 32768) * (h3 / 32768)) / 128;
  h5 = h3 - ((h4 * ((int32_t)cal.dig_H1)) / 16);
  h5 = (h5 < 0 ? 0 : h5);
  h5 = (h5 > 419430400 ? 419430400 : h5);
  const uint32_t H = (uint32_t)(h5 / 4096);
  const float humidity = (float)H / 1024.0;
  out.humidityPercent = (double)humidity;
  return out;
}
// Short images are invalid and are not compensated. Full images use the
// integer formulas, then the short-read gate so a 0 from var1==0 stays valid.
inline Bme280Samples bme280SamplesFromRegisters(const uint8_t *tempPress,
                                               size_t tempPressLen,
                                               const uint8_t *humidity,
                                               size_t humidityLen,
                                               const uint8_t *measurement,
                                               size_t measurementLen,
                                               uint64_t utcMs) {
  Bme280Samples out;
  if (tempPress == nullptr || humidity == nullptr || measurement == nullptr ||
      tempPressLen < 26 || humidityLen < 7 || measurementLen < 8) {
    const size_t gotMeas = measurement == nullptr ? 0 : measurementLen;
    out.temperature = bme280ShortReadSample(0, 1, 0.0, utcMs);
    out.pressure = bme280ShortReadSample(gotMeas, 8, 0.0, utcMs);
    out.humidity = bme280ShortReadSample(0, 1, 0.0, utcMs);
    return out;
  }
  Bme280Calib cal;
  Bme280Adc raw;
  if (!bme280UnpackCalibration(tempPress, tempPressLen, humidity, humidityLen, cal) ||
      !bme280UnpackMeasurement(measurement, measurementLen, raw)) {
    out.temperature = bme280ShortReadSample(0, 1, 0.0, utcMs);
    out.pressure = bme280ShortReadSample(0, 1, 0.0, utcMs);
    out.humidity = bme280ShortReadSample(0, 1, 0.0, utcMs);
    return out;
  }
  const Bme280Compensated comp = bme280Compensate(cal, raw);
  out.temperature = bme280ShortReadSample(tempPressLen, 26, comp.temperatureC, utcMs);
  out.pressure = bme280ShortReadSample(measurementLen, 8, comp.pressurePa, utcMs);
  out.humidity = bme280ShortReadSample(humidityLen, 7, comp.humidityPercent, utcMs);
  return out;
}
} // namespace cts
