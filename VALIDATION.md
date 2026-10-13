# Checkpoint 12 validation — 2026-10-13

`python3 tools/check.py --host /workspace/codontrace-genesis` passed. Host pin remains `74c7b3f15c2d093da03e392094814d63a4997c2c`. No host source was modified. Compiler flags for the native binaries are `-std=c++11 -Wall -Wextra -Wpedantic -Werror`.

| Suite | Result |
|---|---|
| Existing native codec | PASS, checks=117 |
| Command decoder, including 20,000 fuzz inputs | PASS, checks=607 |
| Dispatcher | PASS, checks=1848 |
| Command session | PASS, checks=196 |
| Clock and acquisition | PASS, checks=1101 |
| Independent clock review binary | PASS, checks=50 |
| Network inbox | PASS, checks=2377 |
| Independent network review binary | PASS, checks=87 |
| Millis32 and reconnect/inbox review binary | PASS, 290 checks |
| Pinned host hello, commands, session, clock, network | PASS |
| Nonblocking attempt seam | PASS, checks=108 |
| Sample scheduler, including burst | PASS, checks=36 |
| Six mock drivers | PASS, checks=82 |
| FDC2214 guard | PASS, checks=38 |
| Reconnect orchestrator | PASS, checks=174 |

The previously summed 6,383 native assertions are the first nine rows except the separate 290-check review binary (117+607+1848+196+1101+50+2377+87). They were re-run, not replaced. New checks are 415. These are synthetic protocol and mock-bus tests.

Attack coverage in the new binaries: scheduler burst does not catch up; SHT4x pressure bind/read is unsupported; FDC1004 second CAPDAC is `AlreadyApplied`; FDC2214 register divider 0 is not the divider-1 frequency; FDC211x precise conversion is `NotSupported`; the two FDC2214 frequency paths differ; a failed or QoS-incomplete reply publish does not call a successful `confirmReplyPublished`; disconnect during an in-flight reply increments `losses()` and leaves the reply pending; hello is serialized again before a reading after reconnect; clock-not-ready does not start a connect; invalid BME280, VL53L0X, HX711, FDC1004, and SHT4x results encode as JSON null with `valid:false`, not a valid numeric zero. A connect failure cancels the client before the attempt leaves Running. A handshake the inbox rejects is cancelled before another start. A serialized reading whose publish step fails returns Busy and is retried as those same bytes. An unrepresentable connect deadline does not stay in progress.

| Gate | Actual result |
|---|---|
| Phase 2 | PARTIAL |
| Phase 3 drivers | MOCK_ONLY |
| Phase 4 engine/panel | BLOCKED |
| Phase 5 soak/research | BLOCKED |
| Hardware bench | NOT_RUN |
| ESP32 flash | NOT_RUN |
| Overnight soak | NOT_RUN |
| Scientific RQ | NOT_RUN |
| Compiled on board | NOT_RUN |
| Engine-integrated drivers | NOT_RUN |

`architectures` is not tested board support. Boards under 128 KB SRAM are excluded. `ArduinoMqttAdapter` was not turned into the nonblocking seam and may still block. Mock sensor numbers are SYNTHETIC. No PASS is claimed for bench, flash, soak, or RQ.
