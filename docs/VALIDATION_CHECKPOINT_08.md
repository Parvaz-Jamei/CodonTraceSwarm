# Checkpoint 08 validation — 2026-10-13

- PASS: 6,383 existing native assertions, separately 20,000 parser fuzz inputs.
- PASS: new Millis32 rollover, repeated observation, latched rollback, half-range
  rejection and 10,000 increments spanning multiple wraps.
- PASS: all pinned-host integrations in evidence/native_and_host.log.
- NOT_RUN: Arduino compilation: arduino-cli unavailable; the board runner returned
  code 2 with a NOT_RUN report, preserved in evidence/arduino_compile_not_run.json.
- NOT_RUN: physical boards, sensor drivers, production network adapters, CMake frontend.
- PENDING: independent CP07/CP08 review.

CP07 live TLS broker and sanitizer results are preserved historical evidence, not
new CP08 runs. See docs/VALIDATION_CHECKPOINT_07.md and evidence/broker_lab.
The full core was not changed in this checkpoint; the new header is opt-in. Native
builds cover it, not Arduino SDK/toolchain behavior. No all-board compatibility
claim is established by architectures=* metadata or the presence of sketches.

Scope: Arduino-wide portable architecture. Small-memory profiles, MCU numeric
behavior, exact toolchain/version matrix and hardware qualification remain open.
See docs/ARDUINO_PORTABILITY.md for constraints, reproducing board builds and sources.
