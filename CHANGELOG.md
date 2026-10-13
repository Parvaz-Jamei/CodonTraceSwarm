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
