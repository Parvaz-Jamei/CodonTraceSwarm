# Phase gates — checkpoint 12

`library.properties` lists only architectures with at least 128 KB SRAM. It is **not** tested board support. AVR, megaAVR, SAMD21, UNO R4, ESP8266, Due and other smaller parts are unsupported. No remaining family was compiled, flashed, or bench-tested.

Checkpoint number is not a phase number. Phase 2 stays **PARTIAL**. Phase 3 drivers are **mock-only**. Phase 4 engine/panel and phase 5 soak/research are **BLOCKED**.

| Phase | Status | What this checkpoint actually did |
|---|---|---|
| 1 — core and contract | PARTIAL | Existing codec, grants, and host pin kept. No new memory-map or full-phase PASS. |
| 2 — network, clock, queue | PARTIAL | Native reconnect orchestrator and a separate nonblocking connect seam. `ArduinoMqttAdapter` stays synchronous, may block, and does **not** implement that seam. No board TLS, Wi-Fi, or MCU cancellation. |
| 3 — sensors and FDC | MOCK_ONLY | Six injected-bus mock drivers plus an FDC2214 guard that does not link FDCxCapSense. Only mock-tested may be PASS. |
| 4 — command engine and panel | BLOCKED | No shared engine/panel trace, replay, or hardware sample loop. |
| 5 — soak and research | BLOCKED | No baseline/shadow/active run, no overnight soak, no scientific RQ result. |

| Gate | Status | Why |
|---|---|---|
| Hardware bench | NOT_RUN | No instrumented board. Mock numbers are SYNTHETIC. |
| ESP32 flash | NOT_RUN | No flash, no esptool log, no board compile of this checkpoint. |
| Overnight soak | NOT_RUN | Not started. Do not relabel a short native test as soak. |
| Scientific RQ | NOT_RUN | No preregistered research result. Interoperability tests are not discoveries. |
| Compiled on board | NOT_RUN | Native GCC/Clang only. |
| Engine-integrated | NOT_RUN | Samples can be encoded by `Device`. They are not wired into the Genesis engine or panel. |

A PASS in `docs/DRIVER_STATUS.md` applies only to the mock-tested column. Copying that word into bench, flash, soak, or RQ columns is a false claim.
