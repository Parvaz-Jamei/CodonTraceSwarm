# Driver status — checkpoint 12

Mock numbers are **SYNTHETIC**. A passing native test is not a calibration, a hardware reading, or an engine result. `compiled-on-board`, `bench`, and `engine-integrated` stay **NOT_RUN**. Only `mock-tested` may be PASS.

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
