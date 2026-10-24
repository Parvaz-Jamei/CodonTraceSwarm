# JR1 (unreleased, branch jr1/ci-green-honest; library version still 0.12.11)

No wire or protocol behaviour changed in JR1. Nothing was flashed or bench-tested.

- CI compile matrix (`arduino.yml`): arduino-cli 1.5.1 via SHA-pinned
  `arduino/setup-arduino-cli`; every architecture in `library.properties` compiles every
  example with pinned cores (esp32 3.3.12, rp2040 6.3.0, renesas_portenta 1.6.0, mbed 4.6.0)
  and pinned sensor libraries; nano33ble also on macOS and Windows; `--warnings all`, and any
  warning in library or sketch code fails the job. Arduino lint fixed; macOS bash 3.2 fixed.
  `evidence/arduino_compile_matrix.json` records a CI result and replaces the stale NOT_RUN file.
- All GitHub Actions are pinned by commit SHA.
- PlatformIO native + Unity (`native.yml`): per-module suites (codec, command decoder,
  command session, network inbox, MQTT adapter; 25 tests) under ASan/UBSan.
- Loopback contract against Genesis main pinned at a928ee4: C++ envelopes decode on the host and
  host commands decode in C++. Shared fixtures in `tests/golden` (`messages.jsonl`,
  `commands.jsonl`) plus a depth-64 nested `value` case: C++ rejects it as Capacity; Genesis
  a928ee4 accepts it, reported as KNOWN_GAP on push and a failure in the weekly drift job.
- Vendored Genesis swarm wire fixtures (`tests/golden/genesis_swarm`, Genesis 737d17e,
  `fix/swarm-value-strict`): the contract job checks them against `MANIFEST.sha256` at that
  commit, checks the seven encoder frames against fresh C++ output, and checks the decode outcomes.
- `genesis-drift.yml`: weekly job against the latest Genesis main (runs only once on the default branch).
- `library.json` for PlatformIO with the 7 pinned sensor dependencies; `pio pkg pack` in CI;
  kept in sync with `library.properties`.
- Docs state the device side is MQTT 3.1.1 (ArduinoMqttClient); MQTT 5 is host-only, not implemented here.
- Zero-heap proof for the listed hot paths: `native_noheap` Unity suites with the test binary
  replacing the whole malloc family (incl. reallocarray, memalign, valloc, pvalloc) and every
  operator new form, so glibc- and libstdc++-internal allocations (strdup, asprintf, fopen,
  std::string) are counted; plus `tools/check_no_heap.py` symbol check over library objects and
  header-only adapters. Limits are in README section 2.
- Wokwi ESP32 simulation of CoreSmoke (`wokwi.yml`, `docs/WOKWI.md`): needs `WOKWI_CLI_TOKEN`;
  without it the workflow fails on purpose. Not run yet.
- README CI badges.

- 0.12.11: A hello that keeps failing closes the link after one loss and reconnects on the existing backoff. Readings stay blocked until the new hello completes. Sensor sketches print a local value or the sample status and do not open a codec session. CoreSmoke prints a hello and a reading. Not flashed.
- 0.12.10: A short or missing BME280 calibration no longer becomes a valid pressure of 0. dig_P1==0 is invalid_calibration. Humidity coefficients are decoded without a signed left shift. A Hello or Reading that keeps failing is dropped once after two attempt timeouts, and that budget does not drop a command reply. README examples match the dispatcher API and do not call millis() a UTC clock. Not flashed. PUBACK is still not Genesis acceptance.
- 0.12.9: A publish that keeps accepting bytes is still cancelled 2 attempt-timeouts after it started. BME280 no longer treats an Adafruit getter float as success; calibration and measurement bytes come from a checked Wire read, and a short read is invalid. The two FDC1004 headers cannot be included together. Not Wi-Fi, not MQTT, not flashed. Host acceptance of a reply is still not the same as PUBACK.
- 0.12.7: Sensor examples encode every wrapped quantity. INA219 rejects a failed I2C read. HX711 does not block when the chip is not ready and does not divide by a zero scale. FDC2214 rejects a non-positive caller clock before touching the chip. Native test covers NaN and infinities. Still not flashed.
- 0.12.6: Optional headers call Adafruit BME280, SHT4x, INA219, VL53L0X, the Bogde HX711 library, ProtoCentral FDC1004, and FDCxCapSense. Any other library can pass a finite number through scalarIfFinite. Those sketches compiled for Nano 33 BLE. Not flashed. Mock drivers stay synthetic test doubles.
- 0.12.5: Examples repeat the local Serial line, so opening the monitor after upload still shows it. Still no socket and not flashed.
- 0.12.4: Examples print a local Serial result and say they do not connect or measure. The Library Manager sentence no longer reads like a live sensor product. Still not flashed.
- 0.12.3: PortableClock includes the library header so Arduino IDE on Windows, Linux, and macOS can find Millis32. Examples compiled for Nano 33 BLE; not flashed.
- 0.12.2: Drop boards whose SRAM cannot hold the current profile. Eligible architectures need at least 128 KB. None of them were compiled.
- 0.12.1: CoreSmoke no longer sets wifi=false, so begin/hello can succeed; still no socket.

# Checkpoint 12

- Version 0.12.0. Library URL is https://github.com/Parvaz-Jamei/CodonTraceSwarm.
  That checkpoint still used `architectures=*`, which 0.12.2 replaced.
- Added a single-threaded reconnect orchestrator: fresh hello before readings on
  each generation, reply confirmation only after full write and QoS1, and an
  explicit loss counter when a publish is in flight at disconnect.
- Added a separate nonblocking connect attempt seam (start, partial progress,
  cancel, timeout, stale generation). The synchronous Arduino MQTT adapter does
  not implement it and may still block.
- Independent review rejected an abandoned connect and a second live client after
  an accepted handshake. Failure and close now cancel that client before the
  next start. A serialized reading whose first publish step fails returns Busy
  and is retried, not Failed. An unrepresentable timeout is Fault or TimedOut.
- Added SampleScheduler: set_rate moves only the next deadline, missed slots
  drop instead of bursting, and rates outside the stored channel min/max fail.
- Added six optional mock drivers behind an injected bus. Invalid reads are not
  NaN and are not a valid numeric zero. FDC1004 rejects a second CAPDAC apply.
- Added an FDC2214 guard that does not link FDCxCapSense
  `215031db9a6bc37ac26cd1d2b6dca96bbe3b4da7`. The two frequency paths differ,
  divider 0 is rejected and not replaced by 1, and FDC211x precise conversion
  is NotSupported. Capacitance is not derived without an explicit L and
  parasitic model.
- Hardware bench, ESP32 flash, overnight soak, and scientific RQ stay NOT_RUN.
  Phase 2 remains PARTIAL. Phase 3 is mock-only. Phases 4 and 5 are BLOCKED.
  Mock sensor numbers are SYNTHETIC.
- Connect-attempt failures cancel the client before the phase leaves Running.
  A refused handshake abandons a Succeeded client before policy.lost. A
  serialized reading queued for retry returns Busy, not Failed. An
  unrepresentable connect deadline is Fault at start and TimedOut at poll.

# Checkpoint 11


- Added optional board-neutral ArduinoMqttClient0.1.8 adapter with injected MQTT client.
- Exclusive callback queues concurrent downlink during PUBACK waits; initial polling-only
  design replaced after independent review identified silent loss in upstream interaction.
- Strict retained/topic/QoS/length/short-read checks; foreign-session publication rejected.
- Reply cleared only after complete write and upstream QoS1 PUBACK success.
- Added actual-upstream native contract harness; external LGPL dependency is not bundled.
- Synchronous backend only: no hard realtime, full automatic reconnect or MCU qualification.

# Checkpoint 10

- Connected the actual C++ reconnect policy and inbox to the desktop TLS broker lab.
- Added abrupt broker process termination, observed disconnect callback, two real
  refused connections, policy-controlled retry and authenticated recovery.
- Verified fresh same-boot hello and another actual-host command after recovery.
- Recorded monotonic policy decisions and counters in reconnect_trace.json.
- Prevented stale success reports surviving a failed rerun.
- No production Arduino network adapter, in-flight delivery-loss or hardware claim.

# Checkpoint 09

- Reconciled the newly attached V2 plan: its member content equals the preserved plan.
- Added board-neutral reconnect decisions, capped equal-jitter retry, timeout,
  stale-attempt rejection, clock gating and monotonic fault latching.
- Tested policy events with actual NetworkInbox generation and partial-drop behavior.
- Independent review: 290 checks passed ordinary and ASan/UBSan; no material issue found.
- Fetched current host; documented newer remote SHA without silently replacing test baseline.
- Phase02 remains PARTIAL: production backend, abrupt real-network integration and MCU
  resource/hardware gates remain. No new board or real-broker run claimed.

# Checkpoint 08

- Corrected target scope to all Arduino-supported board families, not ESP32-only.
- Added Arduino library metadata, portable counter and two example sketches.
- Added explicit-FQBN compile runner with FAIL/NOT_RUN separation and evidence output.
- Added wrap/rollback counter regression and documented small-memory/profile gaps.
- No production transport, physical sensor or universal board-compatibility claim.

# Checkpoint 07

- Added a real loopback aMQTT/Paho TLS laboratory with two actual C++ library nodes.
- Verified certificate/hostname checks, authentication, node ACLs, QoS1 replies, duplicate suppression, retained rejection and controlled broker restart.
- Re-ran 6,383 native assertions and host integration; 11 live broker scenarios passed with ASan/UBSan C++ nodes.
- Sanitizer builds now fail on undefined behavior rather than recovering silently.
- Desktop test adapter only; production board adapter and independent CP07 review remain pending.

# Checkpoint 06

- Added fixed-capacity network byte inbox and strict fragment metadata checks.
- Bound network topic scope to the actual CommandSession (independent defect CP06-01 fixed).
- Added explicit connection generations, disconnect loss accounting and backpressure.
- Added 2,377 inbox assertions, 87 independent checks and fragmented real-host roundtrip.
- Network SDK/TLS/board integration remains pending; no live broker qualification.

# Checkpoint 05

- Verified the user-attached checkpoint04 source and preserved target plan V2.
- Added monitored clock metadata, conservative discontinuity gates and health counters.
- Added acquisition stamps bound to the Device session and freshness-gated serialization.
- Added 1,101 clock checks, 50 independent adversarial checks and real-host timestamp integration.
- Documented explicit remaining target-plan gaps; no firmware/network readiness claim.

# Checkpoint 04

- Added frozen per-channel operation grants and registry-backed authorization.
- Added real reply envelopes and CommandSession orchestration with ordered single-slot outbox.
- Added 196 session assertions and actual host/synthetic-callback/reply state integration.
- Tested late positive reply rejection and identical retry semantics.
- Archived checkpoint03 evidence. Network, board clock and physical sensors remain pending.

# Checkpoint 03

- Added injectable clock, bounded local command dispatcher and policy/executor interfaces.
- Added dual deadline checks, latched clock reversal guard, owned scope, semantic deduplication,
  content conflicts, explicit backpressure and callback reentry guard.
- Added 1,848 dispatcher assertions and integrated them into the existing native runner/CMake.
- Preserved checkpoint02 validation and evidence. MQTT, board adapters and ACK remain pending.

# Changelog

## Rebuild checkpoint 02 — 2026-10-13 Asia/Tehran

Adds bounded Unicode-aware command decoding, duplicate/unknown-field rejection, scope/time/parameter gates, owned outputs, malformed-input regressions and actual-host command-generation checks. Prevents copying a live Device codec. Still no dispatch, MQTT or hardware execution.

## Rebuild checkpoint 01 — 2026-10-13 Asia/Tehran

Fresh native core with owned channels, bounded UTF-8 JSON encoding, five sample types,
normalization validation, regression tests and pinned-host interoperability evidence.
This is a cumulative standalone source snapshot. Previous 0.1.0 bytes were not recovered.
Command parsing, transport and hardware layers remain for later snapshots.
