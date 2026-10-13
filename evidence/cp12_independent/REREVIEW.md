# Independent checkpoint 12 re-review

Scope: only the four defects rejected in `evidence/cp12_independent/REVIEW.md`. Product source, coder tests, `CMakeLists.txt`, `tools/check.py`, and `library.properties` were not modified. Host checkout `/workspace/codontrace-genesis` was not modified.

`python3 tools/check.py --host /workspace/codontrace-genesis` exited 0. Pin unchanged: `74c7b3f15c2d093da03e392094814d63a4997c2c`. Coder binaries reported native 117, commands 607 plus 20,000 fuzz inputs, dispatcher 1848, session 196, clock 1101, clock review 50, network 2377, network review 87, millis/reconnect review 290, attempt 108, scheduler 36, drivers 82, FDC2214 38, orchestrator 151, and the pinned host integrations. Hardware bench, flash, soak, and scientific RQ were not run. Docs still say NOT_RUN. That is not a defect.

Probe: `evidence/cp12_independent/rereview_probe.cpp`, compiled outside CMake against the current sources with `-std=c++11 -Wall -Wextra -Wpedantic -Werror` and AddressSanitizer + UndefinedBehaviorSanitizer (`-fno-sanitize-recover=all`). No sanitizer report. Result: 309 checks, 10 failures. Every failure is the remaining defect below. The D1–D4 sections did not fail.

## D1. FIXED

`NonblockingAttempt::poll` calls `client_.cancel` while `phase_` is still `Running`, then leaves `Running`. A later `cancel` is `Stale` and is not the only stop. The next `start` does not stack a second live client.

Minimal input, all three shapes:

```text
start(4, 100, 1000); poll(4, 50)
  -> Fault, phase Failed, cancel saw Running, cancels == 1, live == 0, client.poll not called
  cancel(4) -> Stale, cancels stays 1
  start(5, 50, 1000) -> live == 1, maxLive == 1, not stacked
```

Same for `AttemptIo::Fail` and `done && !ok` (`Failed`, one client poll, then the same cancel / Stale / single next start).

Orchestrated: first client poll is `AttemptIo::Fail` (and, separately, `done && !ok`). After that `orch.poll`, `cancels >= 1`, `live == 0`, policy is `Waiting`. At `nextAttemptMs()`, with the client then able to succeed, `cancels` before the second `start` is already `>= 1`, `live` before that start is 0, `maxLive == 1`.

## D2. FIXED

A `Succeeded` handshake that the inbox rejects is `abandon`ed before another start. `abandon` cancels a `Succeeded` generation. `cancels >= 1` and the previous client is not live.

Minimal inputs: client `done && ok`. Unbound inbox, and separately `bind` plus `connected(50)` so token 1 is `WrongConnection`. `orch.poll(0, true, 0)` then `orch.poll(nextAttemptMs(), true, 0)`.

Observed: after the first poll, `cancels >= 1`, `live == 0`, `generation() == 0`, publisher `steps == 0`. Before the second `start`, `cancels >= 1` and `live == 0`. `maxLive == 1`, not stacked, still no hello bytes.

## D3. FIXED

After `Device::reading` serializes and the first publish step returns false, `publishReading` returns `Busy`, not `Failed`. `health().readings == 0`. `readingsAllowed()` is false while that retry is queued. A second `publishReading` is `Busy` and does not take another sequence. The next `poll` completes one `kind:reading` whose bytes compare equal to that serialization, and the sequence does not move again. `readingsAllowed()` is then true and `readings == 1`.

True `Failed` (unknown channel, 4-byte buffer, invalid sample) does not change the sequence. The polls in between do not publish. A later successful reading is a new payload, once.

`onDisconnected` while a retry is queued still clears it. The next connection's first completed payload is `kind:hello`, not those bytes.

## D4. FIXED

`start(9, UINT64_MAX - 5, 1000)` returns `Fault` with phase still `Idle`. `starts == 0`, `live == 0`. A later `poll(9, UINT64_MAX)` is `Stale`. A normal `start` after that is one live client.

An already-armed unrepresentable deadline (`began = UINT64_MAX - 5`, `timeoutMs = 1000`, phase `Running`) makes `poll(9, UINT64_MAX)` return `TimedOut`. Cancel runs while phase is still `Running`. `client.poll` is not called. `cancels == 1`, `live == 0`. A later poll is `Stale`, not `Progress`. The next `start` does not stack.

Boundary, not the old hole: `start(9, UINT64_MAX - 1000, 1000)` is `Started` (deadline is representable). `poll` at `UINT64_MAX - 1` is `Progress`. `poll` at `UINT64_MAX` is `TimedOut` and cancels. `start(11, UINT64_MAX - 999, 1000)` is `Fault` and does not open a client.

Orchestrator `poll(UINT64_MAX - 5)` with the default 10000 ms attempt timeout does not call `client.start` (`starts == 0`, `live == 0`). The policy then latches `Fault` because the retry delay itself cannot be added. That is fail-closed, not a live client.

## Remaining defect

**DEFECT — an accepted connection is not cancelled, so the next start stacks a second live client.**

D1–D4 cancel or abandon the client they were written for. After the inbox accepts a handshake, `NonblockingAttempt::poll` has already set `Succeeded` and `driveAttempt` does not abandon. `cancel` ignores every phase other than `Running`, so it cannot stop this client. Both teardown paths only cancel a `Running` attempt:

- `ReconnectOrchestrator::handleClose` (`src/cts/ReconnectOrchestrator.cpp`)
- `ReconnectOrchestrator::onDisconnected` (`liveAttempt` requires `AttemptPhase::Running`)

`abandon` is what cancels a `Succeeded` generation, and it is only called when the handshake is refused.

A clock drop while the handshake is still `Running` still cancels (`cancels == 1`, `live == 0`). That path is fine. The hole is after accept.

Minimal input, clock loss while online (policy `Close`; the orchestrator owns the close):

```text
bind inbox; client done && ok
orch.poll(0, true, 0)          -> online, hello complete, cancels == 0, live == 1
orch.poll(1, false, 7)         -> Refused, cancels stays 0, live stays 1
orch.poll(nextAttemptMs(), true, 0)
  -> second start sees live == 1, stacked, maxLive == 2
```

Same stack after an accepted `onDisconnected(generation, 5, 0)` (`OrchCode::Ok`, so the generation was recognized and the link was cleared) and the next due `poll`: `cancels` stays 0, `live` stays 1, `maxLive == 2`.

`docs/RECONNECT_POLICY.md` requires the adapter to cancel/close on `Close`, including clock loss of an active connection. This orchestrator is that adapter. `maxLive == 2` is the same stacked client D1 and D2 were required to prevent.

Previously noted, not this reject: a time rollback while the attempt is still `Running` still cancels (`cancels == 1`, `live == 0`) and latches policy `Fault`, but that same `orch.poll` returns `Ok`. The next poll returns `Fault`. Unchanged, and the Running client is stopped.

## Verdict

**REJECT**

D1, D2, D3, and D4, on their original minimal inputs, are fixed under ASan/UBSan. The accepted-connection close path still leaves the Succeeded client live and the next `start` stacks a second one. Hardware NOT_RUN is unchanged and is not a defect. Do not fix this in this review.
