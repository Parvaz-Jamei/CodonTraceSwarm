# Arduino portability — version 0.12.7

The current profile is not for every Arduino board. `library.properties` lists only
architectures whose boards have enough SRAM for this build. That list is still
**not** a compile, flash, or bench result. The three examples print one local
Serial line and were compiled for Nano 33 BLE only. Nothing was flashed.

## Why boards were removed

Native sizes, host measurement not an MCU map: `Device` 34,928 bytes,
`CommandSession` 12,352, `NetworkInbox` 16,824. Together that is 64,104 bytes,
almost all fixed buffers (`TxBytes` 8192, eight channel records, three 4096-byte
inbox frames plus assembly). A sketch that also keeps a transmit buffer adds
another 8192. On AVR, `double` is 32-bit so `Device` shrinks by about 4 KB and
is still far above 16 KB.

A board stays eligible only when its SRAM is at least 128 KB, leaving headroom
for the core, stack, and radio. No small-memory profile exists, so these parts
are unsupported rather than silently truncated.

| Removed | SRAM | architectures id |
|---|---|---|
| UNO R3, Nano, Leonardo | 2–2.5 KB | avr |
| Mega 2560 | 8 KB | avr |
| Nano Every, Uno WiFi Rev2 | 6 KB | megaavr |
| UNO R4 Minima / WiFi | 32 KB | renesas_uno |
| MKR family, Nano 33 IoT, Zero | 32 KB | samd |
| Due | 96 KB | sam |
| ESP8266 | about 50 KB usable with Wi-Fi | esp8266 |
| STM32F103 Blue Pill and other small STM32 parts | 20 KB class | stm32 |
| nRF52832, Nicla Sense ME | 64 KB | nrf52, mbed_nicla |

SAMD51 boards have enough RAM, but they share the `samd` id with SAMD21, so the
whole architecture is excluded. STM32 parts with 128 KB or more are excluded for
the same reason: one id covers the 20 KB parts.

## Still eligible, not tested

| architectures id | Examples | SRAM | Verified |
|---|---|---|---|
| esp32 | ESP32, S2, S3, C3, C6 | 272 KB and up | NOT_RUN |
| rp2040 | Pico, Pico 2 on the Earle core | 264 KB and up | NOT_RUN |
| mbed_nano | Nano 33 BLE | 256 KB | Compiled with arduino-cli 1.5.1 and mbed_nano 4.6.0, not flashed |
| mbed_rp2040 | Nano RP2040 Connect | 264 KB | NOT_RUN |
| mbed_portenta | Portenta H7 | megabytes | NOT_RUN |
| mbed_giga | Giga R1 | megabytes | NOT_RUN |
| mbed_opta | Opta | megabytes | NOT_RUN |
| renesas_portenta | Portenta C33 | 512 KB | NOT_RUN |

Eligibility is not Wi-Fi or TLS. Optional sensor headers call another library; they were compiled here and not flashed. `CoreSmoke` still constructs a
full `Device` and prints the local hello. On Nano 33 BLE that sketch linked at
91,264 bytes of flash and 87,616 bytes of RAM. `CheckInclude` linked at 85,392
flash and 44,552 RAM. `PortableClock` linked at 85,744 flash and 44,576 RAM.
Nothing was uploaded. The same source is what Arduino IDE builds on Windows,
Linux, and macOS; only Nano 33 BLE was compiled in this environment.

Sensor sketches `Bme280Adafruit`, `Sht4xAdafruit`, `Ina219Adafruit`, `Vl53l0xAdafruit`,
`Hx711Bogde`, `Fdc1004Protocentral`, `FdcxCapSense`, and `AnySensorLibrary` also
compiled for that board after those vendor libraries were installed. That compile
does not mean a chip answered.

## Compile an eligible board

Install that board core first. This script does not install cores, upload, or
touch hardware. A missing CLI is NOT_RUN. A compiler error is FAIL, never PASS.

```sh
python3 tools/check_arduino.py --fqbn YOUR:CORE:BOARD \
  --example CoreSmoke --output build-board-core
python3 tools/check_arduino.py --fqbn YOUR:CORE:BOARD \
  --example PortableClock --output build-board-clock
```

Full qualification still needs a memory map, toolchain pin, and hardware tests.
`PortableClock` does not construct `Device` and does not prove the full profile.

## Generic millis counter

`cts::Millis32` observes an unsigned 32-bit counter. It is not UTC and it does
not make a removed board supported.
