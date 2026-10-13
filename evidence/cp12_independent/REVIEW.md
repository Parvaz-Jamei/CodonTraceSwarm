# Independent checkpoint 12 review

Reviewer scope: read-only on product source, coder tests, CMake, `tools/check.py`, and `library.properties`. Host checkout `/workspace/codontrace-genesis` was not modified. Pin reported by `python3 tools/check.py --host /workspace/codontrace-genesis`: `74c7b3f15c2d093da03e392094814d63a4997c2c`.

Probe: `evidence/cp12_independent/adversarial.cpp`, compiled outside CMake with `-std=c++11 -Wall -Wextra -Wpedantic -Werror` and AddressSanitizer + UndefinedBehaviorSanitizer (`-fno-sanitize-recover=all`). Result: 284 checks, 0 probe failures. Those checks lock the observed product behavior, including the defects below. A clean probe exit is not a product pass.

`tools/check.py --host /workspace/codontrace-genesis` exited 0. Coder binaries matched `VALIDATION.md` (native 117, commands 607 plus 20,000 fuzz inputs, dispatcher 1848, session 196, clock 1101, clock review 50, network 2377, network review 87, millis/reconnect review 290, attempt 36, scheduler 36, drivers 82, FDC2214 38, orchestrator 84) and the pinned host integrations. Hardware bench, flash, soak, and scientific RQ were not run. That is not a defect: the docs say NOT_RUN.

## Required attacks

### 1. Missed scheduler slots — PASS

`src/cts/SampleScheduler.cpp` `poll` selects one due slot, sets `missed` to `late / periodUs`, advances `nextUs` by `(missed + 1) * periodUs`, and returns. A second `poll` at the same time is not due.

Minimal input that holds: channel max 10 Hz, `add("s", 10, 0)`, `poll(100000 + 250 * 100000)`. Result: `due`, `missed == 250`, `totalDrops() == 250`, immediate second poll `due == false` and drops unchanged. A second channel due at the same timestamp is emitted by the next poll only. `poll(UINT64_MAX)` on a 10 kHz slot either faults or emits once; it does not emit twice at that timestamp. `add` of 50 Hz after the caller mutates its own `ChannelSpec.maximumHz` to 100 is still `Rejected` (stored max stays 10). Stored `periodUs` is what `poll` uses.

Non-material observation, not a defect: `add("fast", 9999, 0)` with stored maximum 9999 Hz yields period 100 µs, so the realized rate is 10000 Hz. The limit check is on the requested Hz, which was inside the interval. Time is integer microseconds.

### 2. SHT4x pressure — PASS

`Sht4xReading` has temperature and humidity only. `Sht4x::bind("pressure")` and `readQuantity("pressure")` return `Unsupported`, replace a pre-seeded valid sample with `valid == false` / status `unsupported`, and do not call the bus. Null quantity is `InvalidArgument` and invalid, not a valid zero. A failed bus read is `ReadFailed` with both samples invalid.

### 3. FDC1004 double CAPDAC — PASS

`Fdc1004::applyCapdac` (`src/cts/MockDrivers.cpp`) returns `AlreadyApplied` on a second apply of the same channel, including a second apply of CAPDAC 0 and a different second value. `capdac()` stays at the first value, `rejectedCapdacApplies()` increments, and the bus write count does not increase. A failed first write does not latch, so a later successful apply is allowed. A failed read is `ReadFailed` with `hasPicofarads == false` and an invalid sample, not a valid 0 pF.

### 4. FDC2214 guard — PASS

Under UndefinedBehaviorSanitizer:

- `readFrequencyHzFromRegisters` with divider bits `[9:0] == 0` and `FIN_SEL == 2` returns `InvalidArgument` before any division. `hasFrequency == false`. Its `frequencyHz` is not the divider-1 frequency.
- `readFrequencyHz` uses the caller reference and records `path == CallerReference` and `divider == 0`. It does not divide and does not invent a divider of 1. The same raw and clock with register divider 2 produces half that frequency and `path == Registers`.
- `Fdc2112` and `Fdc2114` return `NotSupported` on both paths, including divider 0 and a present L/parasitic model. `hasFrequency` and `capacitanceDerived` stay false.
- Null model, `present == false`, inductance 0, and negative parasitic leave `capacitanceStatus == "capacitance_not_derived"`.

### 5. Reply confirm and in-flight loss — PASS

`ReconnectOrchestrator::stepPublish` calls `finishPublish` only when `step` returns true, `accepted == size`, `writeComplete`, and `qos1Complete`. These publisher reports leave `replySize` unchanged and `repliesConfirmed == 0`: `step` returns false; `writeComplete` with `accepted == size - 1` and `qos1`; `qos1` with `writeComplete == false`; `writeComplete && qos1` with `accepted == 0`. Write-complete without QoS1 stays `publishActive()` and is not confirmed. `onDisconnected` of the live generation during that publish returns `Lost`, increments `losses()` once, and does not confirm. The same token again returns `Stale` and does not add a loss. While a reading publish is partial, a later `poll` does not rewrite `wire_` (held bytes compare equal). Disconnect of that partial reading is a loss, and the next connection's first completed payload is a hello, not the old reading.

### 6. Hello before readings; clock gate — PASS

`clockReady == false` at due time does not call `ConnectAttemptClient::start` (`starts` stays 0), including a second poll with nonzero jitter. Dropping `clockReady` while a handshake is `Running` cancels that generation (`cancels == 1`) and does not go online. After a new generation, the first completed publish is `kind:hello`. `publishReading` before that hello finishes is `HelloRequired`. A partial reading lost at disconnect is not replayed before the next hello. With an immediate-complete publisher, hello finishes inside the reconnect `poll` and only then does `publishReading` return `Ok` with `kind:reading`.

### 7. Failed read is not a valid zero — PASS

Bus failure on BME280, SHT4x, INA219, VL53L0X, HX711, and FDC1004 yields `valid == false` and a non-`ok` status. Successful raw zeros stay valid: BME280 temperature/pressure/humidity 0; VL53L0X status 0 and millimeter register 0 is `rangeValid` with value 0; HX711 raw equal to tare with a positive span is 0 kg; INA219 counts 0 after `setShuntOhms(0.1)` is 0 A.

### 8. INA219, HX711, VL53L0X — PASS

INA219 before `setShuntOhms` returns `NotConfigured`, `hasCurrent == false`, and an invalid current sample. `0`, negative, and NaN ohms are rejected and do not arm current. HX711 with neither calibration, tare only, or span only returns `hasKilograms == false` and an invalid kilograms sample (including raw 0). Span 0 and infinity are rejected. VL53L0X status 2 is `ReadFailed`, `rangeValid == false`, distance status `timeout`, and the distance sample is not a valid 0 mm even when the distance register is 0. The raw `millimeters` field remains 0 because it is not filled; it is not reported as a valid range.

### 9. Docs and library metadata — PASS

- `docs/PHASE_GATES.md`: phase 2 is PARTIAL, not a full PASS. Phases 4 and 5 are BLOCKED. Bench, flash, soak, and scientific RQ are NOT_RUN. Text says this checkpoint was not bench-tested.
- `docs/DRIVER_STATUS.md`: only the mock-tested column is PASS. Bench and engine-integrated are NOT_RUN. Numbers are described as SYNTHETIC.
- `VALIDATION.md`: phase 2 PARTIAL; no PASS for bench, flash, soak, or RQ.
- `library.properties` `url` is `https://github.com/Parvaz-Jamei/CodonTraceSwarm`.
- `docs/ARDUINO_MQTT_ADAPTER.md` and `src/cts/ArduinoMqttAdapter.h`: the adapter is synchronous and may block, including PUBACK inside `endMessage`. It is not `NonblockingAttempt`.

## Defects the coder tests miss

### D1. DEFECT — a connect client is abandoned with no cancel, and cancel can no longer reach it

`src/cts/NonblockingAttempt.cpp` `poll`. Timeout is the only failure path that calls `client_.cancel`. These exits set a terminal phase and do not cancel:

- `now < began_` returns `Fault` (`NonblockingAttempt.cpp` around the `now < began_` branch).
- `client_.poll` returns `AttemptIo::Fail`.
- `done && !ok` (handshake finished as failure).

`cancel` then returns `Stale` because `phase != Running`, so the client never sees `cancel`. `start` of a new generation calls `client_.start` again.

Minimal input:

```text
start(4, 100, 1000)
poll(4, 50)     -> Fault, client.cancels == 0, phase == Failed
cancel(4)       -> Stale, client.cancels == 0
start(5, 50, 1000) -> client.starts == 2, client.cancels == 0
```

Same shape for `poll` returning `AttemptIo::Fail`, and for `done && !ok`.

`ReconnectOrchestrator::driveAttempt` does not cancel on `AttemptCode::Failed`. It only calls `policy_.lost`. The next due `poll` calls `start` again.

Minimal integrated input: first client `poll` returns `AttemptIo::Fail`. After that `orch.poll`, `cancels == 0` and the policy is `Waiting`. At `nextAttemptMs()`, with the client then able to succeed, `starts >= 2` and `cancels` is still 0.

Orchestrator time rollback is different and does cancel: `ReconnectPolicy::poll` returns `Close` while the attempt is still `Running`, and `handleClose` cancels (`cancels == 1` in the probe). The seam used by itself does not.

Related, lower severity: that same orchestrator `poll` returns `OrchCode::Ok` while latching `LinkState::Fault` (`ReconnectOrchestrator::poll` checks Fault only on entry, then returns Ok/Refused at the bottom). The following `poll` returns `Fault`. Observed: rollback `poll` code `Ok`, `policy.state() == Fault`, `cancels == 1`.

### D2. DEFECT — a successful handshake that the orchestrator refuses is a live connection it cannot cancel

`NonblockingAttempt::poll` moves to `Succeeded` before the owner accepts. `cancel` ignores any phase other than `Running`, so after success there is no API to call `client_.cancel`.

`ReconnectOrchestrator::driveAttempt` on `Succeeded` rolls the policy and inbox back when `inbox.connected` fails, then `clearOnline()`, and does not cancel. The client has already reported the handshake up.

Minimal input: do not bind `NetworkInbox` (or `bind` and `connected(50)` so token 1 is `WrongConnection`). Client `done && ok`. `orch.poll(0, true, 0)` then `orch.poll(nextAttemptMs(), true, 0)`.

Observed: `generation() == 0`, no hello bytes, `cancels == 0`, `starts == 2`. The second `start` is stacked on a client that was never cancelled. `clockReady == false` does not hit this path; a running attempt is cancelled before success is observed.

### D3. DEFECT — `publishReading` can return Failed and still publish that reading on the next poll

`stepPublish` on a failed hello or reading sets `retrySame_` (`ReconnectOrchestrator.cpp`). `publishReading` still returns `Failed` when that first `step` returns false. `service` on the next `poll` calls `startKind(retryKind_)` and sends the same `wire_` bytes. There is no public cancel for that retry short of dropping the link. `readingsAllowed()` does not consult `retrySame_`, so it can be true while a new `publishReading` returns `Busy`.

Minimal input: hello already complete; publisher `step` returns false once; `publishReading` returns `Failed` and `health().readings == 0`; publisher then accepts; the next `poll` completes one reading (`kind:reading`). Observed `completed_delta == 1`.

This does not confirm a reply and does not skip hello after reconnect (`clearOnline` clears `retrySame_`). It does contradict the `Failed` return.

### D4. DEFECT — connect timeout cannot fire in the last `timeoutMs` of the `uint64` clock

`NonblockingAttempt::poll` times out only when `now - began_ >= timeoutMs_`. If `began_ > UINT64_MAX - timeoutMs_`, no legal `now >= began_` can satisfy that, and the attempt stays `Running`.

Minimal input:

```text
start(9, UINT64_MAX - 5, 1000)
poll(9, UINT64_MAX) -> Progress, phase == Running, cancels == 0
```

`SampleScheduler` faults instead of wrapping a deadline near `UINT64_MAX`. This attempt does not. Practical clocks do not reach this range; it is still an overflow hole in the new timeout.

## Looked for, not found

- Scheduler catch-up burst, including two channels and a far-future poll.
- Stale `onDisconnected` closing the current generation or adding a loss.
- In-flight `wire_` alias: a partial publish keeps the same bytes across `poll` until disconnect or completion. `publishReading` copies into `wire_` only when not busy.
- Reply confirm on a short write, QoS1 before write completion, or a zero accepted count.
- Divider 0 replaced by 1, shared FDC frequency path, invented FDC2214 capacitance, or FDC211x frequency.
- Phase 2 marked fully PASS, drivers marked bench-tested, or a scientific result claimed.

## Verdict

**REJECT**

Attacks 1 through 9 hold, and the docs do not over-claim hardware or a scientific result. The new connect seam still drops a live client without `cancel` (D1, D2) and can publish a reading after telling the caller that publish failed (D3). Those are material. Do not fix them in this review.
