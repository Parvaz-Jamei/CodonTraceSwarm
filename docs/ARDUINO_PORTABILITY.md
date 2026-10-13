# Arduino-wide portability target — checkpoint 08

The target is all Arduino-supported board families, not ESP32 alone. Board-neutral
protocol and algorithm code must remain separate from manufacturer SDKs. `architectures=*`
allows Arduino to consider this library on any architecture; it is NOT a tested
compatibility claim. No Arduino board has been cross-compiled or hardware-tested here.

## Layers and acceptance gates

- Portable core: schema, ownership, bounds, grants, command lifecycle and deterministic data rules.
- Platform adapters: monotonic counter, trustworthy UTC observations, transport callbacks,
  TLS credentials, task synchronization and physical sensor drivers.
- Profiles: memory capacity appropriate to each board, preserving explicit overflow/error
  behavior and wire semantics. A tiny profile is not implemented yet.

The current native Device size is 34,928 bytes, CommandSession 12,352 and NetworkInbox
16,824. These are native measurements, not MCU memory maps. This full configuration
cannot reasonably target UNO R3's 2 KB SRAM. Do not merely replace ESP32 with “all boards”
and call compatibility complete. Small AVR boards need a redesigned bounded profile
or a separately specified gateway role. No functionality was silently removed here.

| Family | Target direction | Verified in this package |
|---|---|---|
| AVR / megaAVR | small profile, C++ library/header and number-format checks | NOT_RUN |
| SAM / SAMD | core plus board-specific time/network adapters and RAM budget | NOT_RUN |
| ESP8266 / ESP32 | Arduino core plus secure transport and task handoff | NOT_RUN |
| RP2040 / RP2350 | selected Arduino core, time and network backend | NOT_RUN |
| STM32 / nRF52 / Renesas / other Arduino cores | same explicit compile and resource gates | NOT_RUN |

Arduino support does not imply built-in Wi-Fi. A network-capable board/module or
specified gateway is necessary for the intended network path; the library must not
report Wi-Fi as present when the application has not configured it.

## Compile the selected board

Install your board core using the normal Arduino tools first. This script neither
installs cores, uploads firmware, changes board configuration nor contacts hardware.

```sh
python3 tools/check_arduino.py --fqbn YOUR:CORE:BOARD \
  --example CoreSmoke --output build-board-core
python3 tools/check_arduino.py --fqbn YOUR:CORE:BOARD \
  --example PortableClock --output build-board-clock
```

The report records FQBN, CLI version, installed cores, command, status and log.
Missing CLI produces NOT_RUN with nonzero exit; compile errors produce FAIL, never
PASS. Full qualification additionally requires exact toolchain pinning, memory map,
stack/heap/TLS headroom, numeric serialization checks, hardware time/transport tests
and sustained operation. CoreSmoke does allocate the full Device and output buffer.
PortableClock alone does not validate the full engine integration.

## Generic millis counter adapter

`cts::Millis32` takes an unsigned 32-bit counter, e.g. an explicit cast of `millis()`.
It accumulates elapsed milliseconds from the first observation, handles modular wrap,
and latches a detected rollback/ambiguous half-range step. It has no board dependencies,
no allocation and no network calls. It is single-owner, not interrupt/task-safe.

Observe strictly more often than every 2^31 milliseconds (about 24.85 days); after
deep sleep/reset or an unobserved full wrap, software cannot reconstruct elapsed time
from this counter alone. A small forward jump caused by reset is also undetectable.
Create a new counter and application boot session after reset. Never use elapsed time
as UTC or set `synchronized=true` solely because this counter is ready. Feed a proper
board TimeSource and retain MonitoredClock's age/uncertainty validation.

## Primary sources checked 2026-10-13

- https://docs.arduino.cc/arduino-cli/library-specification
- https://docs.arduino.cc/arduino-cli/sketch-build-process
- https://docs.arduino.cc/arduino-cli/platform-specification/
- https://docs.arduino.cc/resources/datasheets/A000066-datasheet.pdf

These motivate packaging and resource constraints, not an unsupported universality claim.
