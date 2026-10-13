# Checkpoint 01 validation — 2026-10-13 Asia/Tehran

**Checkpoint source is tested; full Phase01 remains PARTIAL.** This report replaces no previous result and makes no claim about the lost 0.1.0 package.

| Gate | Result | Evidence |
|---|---|---|
| C++11 native build with -Wall -Wextra -Wpedantic -Werror | PASS | tools/check.py; evidence/native_and_host.log |
| Behavioral assertions | PASS: 117 checks, 0 failures | tests/native.cpp |
| Actual C++ wire messages | PASS: 7 envelopes, five sample types | evidence/messages.jsonl |
| Real pinned host decoder and coordinator | PASS | tools/verify_host.py; native_and_host.log |
| Same-input coordinator replay | PASS | Fresh coordinator states compare exactly; no engine trajectory claim |
| ASan + UBSan | PASS | evidence/sanitizer.log; LeakSanitizer disabled in local sandbox only |
| CMake frontend | NOT_RUN | Supplied configuration; native compiler entrypoint was executed instead |
| GitHub CI | NOT_RUN | Workflow supplied, not executed in GitHub |
| Strict command parser and duplicate/unknown-key ingress tests | NOT_IMPLEMENTED | Required next for full Phase01 |
| ClockProvider/synchronization/freshness manager | NOT_IMPLEMENTED | Only wire integer bounds and host freshness checked |
| MQTT, board drivers, firmware builds, real hardware | NOT_IMPLEMENTED / NOT_RUN | Future checkpoints |

## Concrete review findings and fixes

1. Quantity/model/version byte bounds originally exceeded the host's ASCII character limits. Restricted to at most 96 bytes; regression includes 97 ASCII characters.
2. Category size could exceed the host's 128-character limit. Restricted to 128 UTF-8 bytes; negative 129-ASCII regression.
3. Normalization evidence IDs must be identifiers, not arbitrary text. Slash/Unicode ID rejection tested.
4. Whitespace-only text was accepted locally but rejected by the host. Added Unicode whitespace classification and regression inputs.
5. An 8 KiB temporary buffer originally consumed call stack. Moved staging into Device-owned storage. Native `sizeof(Device)` is **34,720 bytes** on the tested x86-64 ABI; not a total firmware RAM or peak TLS measurement.

Independent tester authored and ran native tests; manager reviewed host boundaries and ran final native, sanitizer and untouched-host integration checks. Helper agents reached usage limits after producing code/tests; no independent hardware review is claimed. Post-test review A confirms actual behavior against the original host, not a duplicate validator. Post-test review B separates passed codec checks from unimplemented phase gates.

Host source: `74c7b3f15c2d093da03e392094814d63a4997c2c`, fetched remote main matched, checkout unmodified. Host code is not copied into the package.

All sensor fixtures are SYNTHETIC. Their fixed UTC is a controlled input, not a live reading. The local sandbox lacks /proc, so the sanitizer executable was linked with an environment-only override disabling LeakSanitizer. AddressSanitizer and UndefinedBehaviorSanitizer remained active; their symbolization warnings are preserved. The override is not part of library source or normal CI.

Every raw evidence log included here belongs to this checkpoint. Successful tests are finite evidence, not a guarantee of absence of all bugs, universal board support or scientific novelty.
