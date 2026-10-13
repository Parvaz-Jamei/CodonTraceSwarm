# Checkpoint 03 validation — 2026-10-13 Asia/Tehran

Cumulative rebuild snapshot, not a recovered copy of the lost library. Local command
lifecycle is now implemented; full firmware/Phase01 completion is NOT claimed.

| Gate | Result | Evidence |
|---|---|---|
| C++11 warning-clean native build | PASS | tools/check.py; Wall/Wextra/Wpedantic/Werror |
| Core codec assertions | PASS: 117 | tests/native.cpp |
| Command parser assertions | PASS: 607 | tests/commands.cpp |
| Dispatcher lifecycle assertions | PASS: 1,848 | tests/dispatcher.cpp |
| Deterministic parser byte fuzz | PASS: 20,000 inputs | commands suite; separate count |
| Actual pinned-host telemetry compatibility | PASS: 7 envelopes | tools/verify_host.py |
| Actual pinned-host command generation -> C++ decode | PASS: 7 shapes | tools/verify_commands.py |
| ASan + UBSan, all three suites | PASS | evidence/*sanitized.log |
| CMake frontend / hosted GitHub CI | NOT_RUN | build/workflow supplied |
| MQTT/TLS, ACK round trip, board clock adapters | NOT_IMPLEMENTED | next stages |
| ESP32 compilation / physical hardware / FDC driver | NOT_RUN / NOT_IMPLEMENTED | later stages |

**2,572 ordinary assertions** (117 + 607 + 1,848); repeated-cycle assertions are
behavioral checks, not independent scientific experiments. The 20,000 fuzz inputs
are additional synthetic inputs, not 20,000 validated discoveries.

Original host HEAD and fetched origin/main both:
`74c7b3f15c2d093da03e392094814d63a4997c2c`. Host implementation is not bundled or changed.
Command integration remains decode-only; local dispatcher executes synthetic test
callbacks separately. No firmware capabilities or real measurements are fabricated.

## New coverage

FIFO ordering and 100 wrap/reclaim cycles; full queue and full history; live-history
retention even if UTC jumps ahead; duplicate suppression before/after success/failure;
conflicting contents; scope ownership; input reuse; malformed and wrong-scope input;
untrusted/retained rejection; revoked policy; UTC and monotonic expiry independently;
expiry during policy evaluation; temporary lost synchronization; latched clock rollback;
timer addition overflow; clock loss during execution gate; reentrant callback rejection.

No callback runs for expired or denied commands. Failed callbacks are terminal.
Tests exercise 800 sequential callbacks in the wrap/reclaim scenario, not physical actuators.
See docs/DISPATCHER.md for bounded-history and reboot limitations.

## Sanitizer limitations and review

Environment lacks /proc; local-only override disables LeakSanitizer while ASan/UBSan
remain active. Symbolization warnings are preserved. The override is outside the
package and normal CI. Leak-free behavior is not claimed.

Native sizeof(Device)=34,720 and sizeof(Dispatcher)=11,000 bytes; not board memory
measurements. Objects and their adapters are single-owner, single-event-loop only.

Implementation and review in this checkpoint were performed by the primary agent;
independent tester approval is PENDING. Design and code/result self-review are recorded
in notes/DESIGN_REVIEW.md. No claim of release readiness or absence of every bug.
