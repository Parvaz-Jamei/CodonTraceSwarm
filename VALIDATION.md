# Checkpoint 06 validation — 2026-10-13 Asia/Tehran

Cumulative portable library; checkpoint05 and target plan were available and reused.

| Gate | Result | Evidence |
|---|---|---|
| Native C++11 warnings-as-errors | PASS | tools/check.py |
| Existing six suites | PASS: 3,919 assertions | core/parser/dispatcher/session/clock/independent clock |
| Network inbox suite | PASS: 2,377 assertions | tests/network.cpp |
| Independent network suite | PASS: 87 assertions | tests/network_review.cpp |
| Parser fuzz | PASS: 20,000 inputs, separate | commands suite |
| All previous actual-host integrations | PASS | current native_and_host.log |
| Fragmented receive -> session -> actual host | PASS | network_roundtrip.json |
| ASan/UBSan | PASS: eight suites and fragmented round-trip probe | *sanitized.log |
| Independent review | COMPLETED within described scope | evidence/independent_network_review/ |
| CMake frontend / hosted CI / MCU builds | NOT_RUN | configurations supplied |
| Live MQTT broker / TLS / board adapters | NOT_IMPLEMENTED / NOT_RUN | target work remains |

**6,383 ordinary assertions** (3,919 + 2,377 + 87), plus 20,000 synthetic fuzz inputs.
Repeated partition/boundary checks are not independent scientific experiments.

Fetched host remains 74c7b3f15c2d093da03e392094814d63a4997c2c. Host source is not changed
or included. Host-generated commands enter the new inbox in 7-byte fragments, then reach
actual C++ session callbacks and the real host reply path. Sensor value remains synthetic.

## Found and fixed

Independent tester identified CP06-01: an inbox bound to one topic scope could originally
submit a matching JSON body to a different CommandSession scope. Added live matchesRoute
verification before draining. Independent regression passes; wrong-session input is
retained with explicit ScopeMismatch and invokes no command.

Other checked cases: retain unknown/true, untrusted delivery, wrong exact topic, stale
connection generation, payload ownership, gaps/overflow, queue full, retry on unavailable
clock, disconnect/reconnect loss accounting, malformed JSON and pending-reply ordering.

## Limits

Native NetworkInbox size is 16,824 bytes; no TLS/board RAM claim. The class is NOT
thread-safe; a real SDK callback/main-loop integration needs external serialization.
Metadata flags are adapter assertions, not authentication implemented by this layer.
Partial frames have bounded memory but no internal time-based assembly timeout; new
starts/disconnect discard them. Target retry-window and admission wire-reply gaps remain.

ASan/UBSan ran with the documented sandbox-only LSan disable helper because /proc is
unavailable. LeakSanitizer NOT_RUN; symbolization warnings retained. Independent review
covers native code/tests, not broker behavior, firmware or release qualification.
