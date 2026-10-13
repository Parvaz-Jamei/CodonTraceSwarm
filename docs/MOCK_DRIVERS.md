# Mock drivers — checkpoint 12

These classes are test doubles for the native suite. They are not the board path. A real chip goes through `src/cts/sensors/` and that chip's own library. See [DRIVER_STATUS.md](DRIVER_STATUS.md).

`cts::ByteBus` is a test seam with `write`, `read`, and failure. It is not `Wire.h` and not an I2C driver. The six drivers are optional, native C++11, and free of heap, `String`, and threads. Every count below is **SYNTHETIC**.

| Driver | Behavior under test |
|---|---|
| BME280 | Temperature, pressure, and humidity from a fake register image. Not-ready and read failure are invalid samples: not NaN and not a valid zero. A real zero reading stays valid. |
| SHT4x | Temperature and humidity only. Binding or reading `"pressure"` returns `Unsupported` and does not touch the bus. |
| INA219 | Signed shunt and bus counts. Amperes exist only after `setShuntOhms` with a finite positive value. Otherwise there is no current sample. |
| VL53L0X | Millimeters plus `rangeValid`. Timeout and not-ready are invalid, including when the distance register is 0. |
| HX711 | Signed 24-bit raw, including the negative edge `-8388608`. Kilograms only when both tare and a non-zero span are set. |
| FDC1004 | Signed 24-bit raw and CAPDAC. A second `applyCapdac` on the same channel returns `AlreadyApplied`, keeps the first value, and increments `rejectedCapdacApplies`. Read failure is invalid, not 0 pF. |

Successful `cts::Sample` values can be passed to the existing `Device` encoder. Invalid samples encode as JSON null with `valid:false`. See `docs/DRIVER_STATUS.md`. Nothing here is bench-tested or engine-integrated.
