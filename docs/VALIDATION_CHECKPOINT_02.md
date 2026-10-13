# Checkpoint 02 validation — 2026-10-13 Asia/Tehran

This cumulative snapshot extends checkpoint01. Its 117 original checks still pass.
Full Phase01 remains PARTIAL (ClockProvider and remaining exit criteria are not all complete).

| Gate | Result | Evidence |
|---|---|---|
| C++11 native warning-clean build | PASS | tools/check.py, -Wall -Wextra -Wpedantic -Werror |
| Existing core regressions | PASS: 117 checks | tests/native.cpp |
| Command tests | PASS: 607 checks | tests/commands.cpp |
| Parser fuzz exercise | PASS: 20,000 deterministic byte inputs | tests/commands.cpp |
| Original-host telemetry integration | PASS: 7 C++ envelopes, five value types | tools/verify_host.py |
| Original-host command generation -> C++ decoding | PASS: 7 operation shapes | tools/verify_commands.py; evidence/commands.json |
| Native and command ASan + UBSan | PASS | evidence/sanitizer.log; commands-sanitized.log |
| CMake frontend and GitHub-hosted CI | NOT_RUN | Configurations supplied; direct compiler workflow executed |
| Clock provider/sync, queues, deduplication and execution | NOT_IMPLEMENTED | Next checkpoint |
| MQTT/TLS, drivers, ESP32 compilation, physical hardware | NOT_IMPLEMENTED / NOT_RUN | Later checkpoints |

Total ordinary assertions: **724** (117 + 607). The fuzz loop is counted separately,
not inflated into 20,000 independent scientific tests. All real-host checks use original
code at 74c7b3f15c2d093da03e392094814d63a4997c2c; fetched main still matched and checkout
was not modified. No host engine implementation is included in this MIT package.

## Boundaries covered

Duplicate keys including Unicode escapes and raw/escaped Unicode equivalents; nested
duplicates; unknown fields; incomplete input at every prefix; input size and nesting/token
limits; invalid UTF-8 and lone surrogates; embedded NUL; malformed number grammar;
nonfinite/out-of-range values; integer versus float/bool timestamps; session scope;
future/expired/excessive lifetimes; parameter shape and source digest; output reset and
ownership; full parser reuse after bad input. Core sample/normalization/ownership tests
remain unchanged. Copying Device is now disallowed to prevent accidentally cloning a
live sequence state.

## Actual footprint and limitations

Native x86-64 sizeof(Device)=34,720 bytes; sizeof(CommandDecoder)=6,456 bytes. These are
object sizes, not a firmware memory map or total TLS/stack peak. Use long-lived storage
for both objects on constrained devices. Parser decoding allocates no heap; the tests
and subprocess harness do allocate ordinary host memory.

The local environment lacks /proc. A local-only link override disabled LeakSanitizer;
AddressSanitizer and UndefinedBehaviorSanitizer stayed enabled. Symbolization warnings
are retained in logs. The override is not shipped in core or normal CI. Leak-free behavior
is not claimed.

Command interoperability uses a **synthetic controller-side capability fixture** to ask
the actual host to generate all seven shapes. The production codec still advertises an
empty operations list because no dispatcher exists. Decoding success is not execution,
authentication, replay prevention, exactly-once delivery or physical sensor validation.
The seven source commands and C++ decoded values are archived in evidence/commands.json.

Manager implemented and tested this checkpoint after helper quota exhaustion; independent
review is not claimed. Prior independent native-test work remains in checkpoint01 history.
Sources, five design decisions and review boundaries are in notes/. Future transport must
recheck expiry at execution time and enforce advertised capabilities and broker/topic scope.
