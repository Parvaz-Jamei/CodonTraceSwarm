# Checkpoint 05 validation — 2026-10-13 Asia/Tehran

Authoritative input: user-attached checkpoint04 ZIP, every SHA256SUMS entry verified;
working source matched every entry. Target plan V2 is preserved in planning/.

| Gate | Result | Evidence |
|---|---|---|
| Native C++11 warnings-as-errors | PASS | tools/check.py |
| Existing suites | PASS: 117 + 607 + 1,848 + 196 | core/parser/dispatcher/session |
| Monitored-clock/acquisition suite | PASS: 1,101 | tests/clock.cpp |
| Independent adversarial clock suite | PASS: 50 | tests/clock_review.cpp |
| Parser fuzz | PASS: 20,000 synthetic byte inputs | counted separately |
| Actual pinned-host old integrations | PASS | telemetry, commands and callback/reply round-trip |
| Monitored acquisition -> actual host | PASS | delayed acquisition timestamp, zero/null, stale gate |
| ASan + UBSan | PASS: six suites + monitored-acquisition probe | evidence/*sanitized.log |
| Independent review | COMPLETED for described native scope | evidence/independent_review/ |
| Hosted CI / CMake frontend / MCU compile | NOT_RUN | configurations supplied |
| NTP / board time adapter / MQTT/TLS / hardware | NOT_IMPLEMENTED / NOT_RUN | see target status map |

**3,919 ordinary assertions**, including repeated synthetic observations, plus 20,000
fuzz inputs. Counts are not independent scientific experiments or hardware soak evidence.

Host HEAD and fetched main: `74c7b3f15c2d093da03e392094814d63a4997c2c` (unmodified).
Real host fixtures use its actual code; no host implementation is bundled into MIT source.
The new C++ probe records UTC at acquisition, advances synthetic time by 100 ms, and
serializes the ORIGINAL timestamp. After 1,100 ms against a 1,000 ms gate, its old sample
is rejected without a sequence increment; a fresh invalid/null sample is accepted by host.

Clock metadata comes from a coherent trusted TimeSource. Unknown uncertainty is rejected
by default, not guessed. Discontinuities latch; bad/stale metadata suspends ready state.
Telemetry gate rejects mismatched timestamps, foreign/stale/previous-session stamps.
Health counters describe actual gate decisions, not estimated sensor accuracy.

## Memory and limits

Native x86-64 sizeof(Device)=34,928 (+8 from checkpoint04), MonitoredClock=144,
TimedTelemetry=72 bytes. CommandSession remains 12,352. These exclude TLS/transport,
stack and driver buffers; no board memory-map or zero-allocation TLS claim.

Local ASan/UBSan runs use a sandbox-only LeakSanitizer disable helper because /proc is
unavailable. Symbolization warnings are retained. LeakSanitizer NOT_RUN. No failed tests
were relabeled PASS. Independent tests and review were authored by a separate tester;
review is bounded to native code and does not constitute hardware/release qualification.

See docs/TARGET_PLAN_STATUS_FA.md for explicit remaining requirements. In particular,
16-vs-32 history capacity, extra retry-window retention and wire replies for rejected
admissions are still open. Checkpoint05 is NOT target Phase05 completion.
