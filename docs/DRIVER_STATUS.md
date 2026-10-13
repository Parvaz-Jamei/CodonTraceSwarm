# Driver status — checkpoint 12

Mock numbers are **SYNTHETIC**. A passing native test is not a calibration, a hardware reading, or an engine result. `compiled-on-board`, `bench`, and `engine-integrated` stay **NOT_RUN**. Only `mock-tested` may be PASS.

The board path is separate. `src/cts/sensors/` calls the sensor's own Arduino library. A non-finite result becomes an invalid sample, not a numeric zero. `utcMs` is still caller-supplied. Nothing below was flashed or read from a chip.

| Vendor library | Header | Nano 33 BLE compile |
|---|---|---|
| Adafruit BME280 Library 2.3.0 | `cts/sensors/Bme280Adafruit.h` | compiled, not flashed |
| Adafruit SHT4x Library 1.0.5 | `cts/sensors/Sht4xAdafruit.h` | compiled, not flashed |
| Adafruit INA219 1.2.3 | `cts/sensors/Ina219Adafruit.h` | compiled, not flashed |
| Adafruit_VL53L0X 1.2.5 | `cts/sensors/Vl53l0xAdafruit.h` | compiled, not flashed |
| HX711 Arduino Library 0.7.5 | `cts/sensors/Hx711Bogde.h` | compiled, not flashed |
| ProtoCentral FDC1004 2.0.1 | `cts/sensors/Fdc1004Protocentral.h` | compiled, not flashed |
| FDCxCapSense 0.6.13 | `cts/sensors/FdcxCapSense.h` | compiled, not flashed |
| Any other library | `cts/SensorSample.h` `scalarIfFinite` | compiled via AnySensorLibrary, not flashed |

Other BME280 or HX711 forks are not wrapped. Pass their number to `scalarIfFinite`.

| Driver | mock-tested | compiled-on-board | bench | engine-integrated |
|---|---|---|---|---|
| BME280 | PASS | NOT_RUN | NOT_RUN | NOT_RUN |
| SHT4x | PASS | NOT_RUN | NOT_RUN | NOT_RUN |
| INA219 | PASS | NOT_RUN | NOT_RUN | NOT_RUN |
| VL53L0X | PASS | NOT_RUN | NOT_RUN | NOT_RUN |
| HX711 | PASS | NOT_RUN | NOT_RUN | NOT_RUN |
| FDC1004 | PASS | NOT_RUN | NOT_RUN | NOT_RUN |
| FDC2214 guard | PASS | NOT_RUN | NOT_RUN | NOT_RUN |

The FDC2214 row is a local guard, not a link to [Parvaz-Jamei/FDCxCapSense](https://github.com/Parvaz-Jamei/FDCxCapSense) commit `215031db9a6bc37ac26cd1d2b6dca96bbe3b4da7`. That library is not vendored here.

`Device` encoding of an invalid mock sample checks the codec (null, `valid:false`, not a numeric zero). That is not engine integration and does not fill the last column.
