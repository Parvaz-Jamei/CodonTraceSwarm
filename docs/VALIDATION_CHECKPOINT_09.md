# Checkpoint 09 validation — 2026-10-13

| Check | Actual result | Evidence |
|---|---|---|
| Existing core suites | PASS: 6,383 assertions | evidence/native_and_host.log |
| Parser fuzz | PASS: 20,000 deterministic inputs | same log |
| Millis32 regression | PASS | same log |
| Reconnect policy / actual NetworkInbox | PASS: synthetic disconnect/generation events | same log |
| Independent reconnect + millis boundary review | PASS: 290 assertions | evidence/cp09_independent |
| Independent new-header ASan/UBSan | PASS; LSan not run | same directory |
| Pinned Genesis integrations | PASS | evidence/native_and_host.log |
| CP09 real sockets / Arduino compile / physical board | NOT_RUN | not inferred from unit tests |
| CP07 broker review | still PENDING | outside CP09 review scope |

No claim that 6,673 assertions are independent scientific experiments. New integration
assertions and Millis32 loop checks are separately described rather than inflating
that count. Normal runner now includes independently authored reconnect_review.

The uploaded V2 plan has identical member hashes to planning/Target_Plan_V2.zip;
see notes/PLAN_RECONCILIATION_CP09.json. Host test pin remains
74c7b3f15c2d093da03e392094814d63a4997c2c; git fetch found current remote main
fa7e7e995afe3ad7c0970ed783948d0336ac91f3. Relevant diff inspected, newer host not
separately integration-tested. Original host source was not modified.

Policy decisions must be consumed by a nonblocking, task-safe, certificate-verifying
backend. That backend is not supplied here. Earlier CP07 TLS evidence is preserved
historical evidence, not proof the new policy has driven a real reconnect. MCU
memory maps, small AVR profiles, physical drivers and research trials remain open.
