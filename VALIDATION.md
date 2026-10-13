# Checkpoint 04 validation — 2026-10-13 Asia/Tehran

Cumulative portable library rebuild. New layer: registry-backed operation grants,
actual wire replies and CommandSession with a bounded ordered reply outbox.

| Gate | Result | Evidence |
|---|---|---|
| C++11 warning-clean direct build | PASS | tools/check.py; Wall/Wextra/Wpedantic/Werror |
| Core codec assertions | PASS: 117 | tests/native.cpp |
| Parser assertions | PASS: 607 | tests/commands.cpp |
| Dispatcher assertions | PASS: 1,848 | tests/dispatcher.cpp |
| Session/registry/reply assertions | PASS: 196 | tests/session.cpp |
| Deterministic parser fuzz | PASS: 20,000 inputs, separate count | commands suite |
| Actual pinned-host telemetry | PASS: 7 envelopes | verify_host.py |
| Actual pinned-host command decode | PASS: 7 command shapes | verify_commands.py |
| Actual pinned-host callback/reply round trip | PASS: success/failure/expiry/duplicate/late-reply cases | verify_session.py; roundtrip.json |
| ASan + UBSan | PASS: four suites + round-trip probe | evidence/*sanitized.log |
| CMake frontend / hosted CI | NOT_RUN | configuration supplied |
| MQTT/TLS, real board clock, Wi-Fi reconnect | NOT_IMPLEMENTED | later stages |
| Firmware builds / physical sensors / FDC driver | NOT_RUN / NOT_IMPLEMENTED | later stages |

**2,768 ordinary assertions**, plus 20,000 synthetic fuzz inputs. Repeated assertions
and fixed fixtures are software checks, not independent scientific experiments.

Original host HEAD and fetched main are both
`74c7b3f15c2d093da03e392094814d63a4997c2c`; working tree clean and unmodified.
The command round-trip uses actual host code, actual C++ Device/CommandSession and
an explicitly synthetic sample callback (42.5). Communication uses subprocess pipes,
not a broker. Hello capabilities in this round-trip come directly from Device grants.

Host terminal statuses for four commands: acknowledged, rejected, rejected, expired.
The final command succeeded locally but its late positive reply was correctly refused
by the host. Negative controls are intentional expected behavior, not failed test gates.

## Changes and review findings

Device now stores type/rate/grant metadata once for both wire advertisement and policy.
Default operations stay empty. Operation enum moved to the public main header without
renaming its namespace or values. See below for the exact memory comparison; no board-memory inference is made.

Historical checkpoint03 Device size: 34,720 bytes. Current: **34,920 bytes** (+200).
Current CommandSession: **12,352 bytes**, including its dispatcher and reply outbox.
Callbacks/referenced objects must outlive the session. Neither object is thread-safe.

A first test incorrectly required newline to serialize as backslash-n. The existing
writer correctly emits Unicode escape U+000A. The assertion was corrected to check
that valid representation; the writer was not changed or bypassed.

Review addressed pending-reply ordering, stale publication confirmation, state loss
on codec restart, and execution expiry while waiting behind an unsent reply. Admission
rejections still have local errors only; host rejection replies are not fabricated.

## Limits and remaining review

ASan/UBSan passed with the same sandbox-only LeakSanitizer disable override; /proc is
unavailable and symbolization warnings are retained. Leak-free behavior is not claimed.
Native GCC compilation does not establish ESP32 build compatibility.

Independent reviewer/tester approval remains PENDING. Primary-agent implementation,
self-review and executed tests are documented; release readiness is not claimed.
Prior reports/logs are archived in docs/ and evidence/checkpoint01 through checkpoint03.
