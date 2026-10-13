# Checkpoint 10 validation — 2026-10-13

| Gate | Result | Evidence |
|---|---|---|
| Existing 6,383 native assertions, 20,000 fuzz inputs | PASS | evidence/native_and_host.log |
| Millis32 / policy-inbox / independent290 native regression | PASS | same log |
| Real TLS broker lab | PASS: 13 scenarios | evidence/broker_lab_cp10 |
| Independent rerun of current lab | PASS: 13 scenarios | evidence/cp10_independent |
| C++ live broker node ASan/UBSan | PASS: 13 scenarios | evidence/broker_lab_cp10/sanitized |
| Arduino compilation / boards / physical sensors | NOT_RUN | no qualification claimed |

Independent review found no material defect within inspected bridge/runner/policy scope.
Retry trace verifies actual attempts were not earlier than their deadlines. Four policy
attempts include initial adoption of an existing connection and three new connection
attempts; three failures include original loss and two actual socket refusals. New
recovery path covers nodeA; existing ACL/two-node tests still run. This is not a fleet test.

ASan/UBSan recovery disabled. LSan disabled only through external sandbox helper because
/proc is unavailable; symbolization warnings preserved. Ordinary core suite rerun; no
claim that all core sanitizer suites were rerun in CP10.

Sensor=synthetic42.5, fixed application UTC, asserted clockReady and zero jitter remain
fixtures. TLS certificate/hostname verification and sockets are real. Python reconnect
calls are synchronous on loopback. Broker is killed with connected clients but no
publication deliberately held in flight. No production Arduino/nonblocking adapter,
NTP qualification, persistent-session or in-flight PUBACK recovery claim.

Pinned tested host74c7b3f15c2d093da03e392094814d63a4997c2c; fetched remote remains
fa7e7e995afe3ad7c0970ed783948d0336ac91f3. Original host not modified or pushed.
Phase02 remains PARTIAL. See RESUME_FA.md and docs/BROKER_LAB.md for next work.
