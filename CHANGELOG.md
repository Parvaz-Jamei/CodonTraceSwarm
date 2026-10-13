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
