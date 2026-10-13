# Checkpoint 01 design record

Date: 2026-10-13 Asia/Tehran. Roles: manager/integration, core coder, independent native tester. These are design decisions, not votes or fabricated performance results.

## Five design passes

1. **Recovery boundary.** Rebuilding the lost 0.1.0 byte-for-byte cannot be verified. Start a clearly numbered rebuild; do not copy its old PASS numbers. Every ZIP is a full cumulative source tree.
2. **Ownership and capacity.** Borrowed descriptor strings risk lifetime bugs; copy registry data into bounded storage. Whole hello must fit before a session starts. Maxima per field do not imply every combination fits.
3. **Measurement meaning.** Wire values retain source units. Validate optional transformed ranges without sending the converted value twice. Invalid observations use null; a real zero stays valid.
4. **Codec versus transport.** This checkpoint serializes envelopes and consumes sequences only on successful serialization. It does not own MQTT delivery; retrying a serialized envelope must reuse the same bytes. Future transport needs explicit attempt/retry semantics.
5. **Scope gate.** Do not advertise command operations or real Wi-Fi connectivity merely because a message can be encoded. The wire profile declares Wi-Fi eligibility; caller must enforce it on hardware. Full Phase01 remains open until command parser and all corresponding negative tests exist.

## Review passes

Pre-test review A: derive tests from host contract, not only implementation. Test malformed descriptors, byte limits, invalid UTF-8, serialization escaping, ownership and all value types.

Pre-test review B: distinguish phase criteria from checkpoint deliverables; defer MQTT, hardware, drivers and command execution explicitly. Original host source remains separate and unmodified.

Post-test reviews and concrete findings are recorded in VALIDATION.md and tester notes. No skipped test may be renamed PASS.

# Checkpoint 02 design/review

Five decisions: (1) keep parsing separate from execution; (2) fixed 4096-byte,96-token,
8-depth limits; (3) decode escapes before duplicate-key comparison; (4) integer UTC and
explicit session scope; (5) owned command outputs cleared on failure. Real host-generated
commands serve as interoperability inputs, while explicit attack cases test rejection.

Review A: nested and escaped duplicate keys, unknown fields, integer/boolean distinction,
UTF-8/surrogates and all truncated prefixes. Review B: successful parsing is not hardware
capability or authorization; hello operations remain empty and dispatch stays deferred.
Post-test A compares all seven shapes with original host generation; post-test B checks
sanitizer output, retains old regressions and marks physical/network gates unimplemented.

Independent helper agents were unavailable due to quota for this checkpoint. The manager
implemented and verified it directly. This is not represented as independent code review.

# Checkpoint 03 — five design decisions and self-review

1. Inject coherent UTC/monotonic snapshots; do not emulate SNTP or MCU timer behavior.
2. Parse once into owned values; FIFO/history retain no caller buffer references.
3. Keep accepted IDs through their complete valid interval; backpressure instead of live eviction.
4. Separate policy and executor; recheck policy and both time gates immediately before callback.
5. Single-owner, no reset/copy, no implicit retry; recovery and network ACK remain explicit.

Code review A: identified that checking time only before policy could execute a command
that expired inside policy evaluation. Added second snapshot and dedicated regression.
Code review B: separated successful begin/clock status (Ok) from actual admission (Queued);
checked overflow, input ownership, callback reentry and no live cache eviction.
Result review A: 1,848 dispatcher assertions including 100 wrap/reclaim cycles; no failed
callbacks retried. Result review B: all old regressions and host compatibility preserved;
ASan/UBSan passed with the documented environment limitation. No hardware inference.
Independent review remains PENDING; these are primary-agent review passes, not a second tester.

# Checkpoint 04 — five design decisions and review passes

1. Device owns channel type/rate metadata and immutable operation grants; RegistryPolicy
   reads it directly rather than maintaining a separate constraint table.
2. Move Operation enum into the public codec header so grants and decoder share values.
3. Use actual Genesis reply schema with command_id; Queued is never ok=true.
4. One bounded outbox retains exact bytes/sequence; block later Device serialization
   and callback execution until explicit ordered-transport confirmation.
5. Keep failure/expiry and host-late-reply cases in the real-host integration; do not
   turn synthetic sensor readings into a physical measurement claim.

Code review A: identified shared-sequence overtaking if telemetry bypassed a pending
reply. Added Device serialization hold and wrong-sequence confirmation tests.
Code review B: session identity changes inside/outside callbacks must stop old-session
replies. Added scope guards and callback-reentry checks; retained RAM-only limitations.
Result review A: all 2,768 assertions passed; an initially overspecific newline test
was corrected to the writer's valid Unicode escape form (no writer behavior changed).
Result review B: actual host terminal statuses acknowledged/rejected/rejected/expired,
including intentional refusal of late success. All four native suites and real-host
probe passed ASan/UBSan with the documented LeakSanitizer environment override.
Independent review remains PENDING; these are primary-agent self-review passes.

# Checkpoint05 — five design decisions

1. Coherent injected TimeSource snapshots; portable gates do not pretend to be NTP.
2. Unknown metadata remains unknown; explicit opt-out cannot rewrite uncertainty as zero.
3. Watch monotonic rollback even during unsynchronized samples; latch mapping discontinuities.
4. Capture UTC at acquisition and bind it to the Device session; never retimestamp a backlog.
5. Keep low-level codec separate from monitored telemetry and document bypass boundaries.

Pre-test review A: source provenance and boundary inclusivity. Pre-test review B: clock
outages must not hide rollback, stamps must reject foreign/old sessions, rejected data
must consume no sequence. Independent tester authored 50 adversarial checks and reviewed
the new production code; see evidence/independent_review. No reproducible blocker found.
Result review A: all old regressions plus new monitor/gate tests and sanitizers passed.
Result review B: real host accepted delayed acquisition timestamps/zero/null; stale input
was filtered without replacing its UTC. No board or NTP result inferred.

Target-plan comparison made explicit: profile history16 versus suggested32, additional
retry-window retention, and rejection wire replies remain gaps. Do not call this Phase05.

# Checkpoint06 — five design decisions and review

1. Copy bounded receive bytes only in callback path; defer parsing/execution.
2. Carry connection generation, explicit retain knowledge and exact topic lengths.
3. Reassemble one contiguous publish with bounded storage; count abandon/disconnect loss.
4. Preserve head frame under transient backpressure; do not evict queued accepted data.
5. Bind topic scope to actual live CommandSession, not merely independently valid strings.

Pre-test A reviewed ESP-MQTT fragment semantics; pre-test B reviewed memory/ownership and
single-owner task contract. Independent tester found CP06-01 scope mismatch and verified
its fix with a regression. Added reentry guard for callback exposure while polling;
external task synchronization remains required. Result review A: all eight native suites
pass, including independent87. Result review B: real host-generated commands split into
7-byte chunks still yield correct success/failure/expiry/duplicate outcomes. Sanitizers
passed with documented LSan limitation. No live network/firmware qualification inferred.


## CP09 — five focused design decisions (primary author)

1. Keep transport SDK outside policy so Arduino families share semantics.
2. Use single-owner monotonic state; stale callbacks keyed by attempt token are ignored.
3. Gate every connection on valid clock; bound attempts rather than blocking sleeps.
4. Cap exponential equal-jitter windows and reset only after stable connection, avoiding flapping reset.
5. Exercise actual NetworkInbox loss/generation behavior, not only a standalone timer.

Pre-test critique A: modulo wrap cannot manufacture UTC; caller must use actual validated clock.
Pre-test critique B: canceling a stale callback must target its own socket, not the current one.
Independent tester assigned production review and separate adverse tests. Results appended
once actually executed; no multi-round/team sign-off claimed in advance.


CP09 result review A: primary native integration passed with real NetworkInbox;
existing 6,383 assertions, 20,000 fuzz inputs, millis regression and pinned host paths
remained green. Review target added to normal runner and CMake; independent binary
rerun after integration also passed 290 assertions.
Result review B: independent native and ASan/UBSan tests found no material defect.
Synthetic event tests cannot stand in for real socket cancellation, MCU clocks,
network dropout or fleet jitter. Independent review scope is these headers, not a
retroactive CP07 broker release sign-off. Phase02 remains partial.


## CP10 design decisions, primary author

1. Reuse the actual broker, nodes, grants and host; no duplicate fake protocol model.
2. Expose a controlled test-only policy bridge; production policy source is unchanged.
3. Terminate broker with clients online; require actual disconnect callback before lost.
4. Keep broker offline for two genuine refused attempts; do not force policy deadlines.
5. Require verified TLS/subscription, fresh hello and real host acceptance after recovery.

Pre-test critique A: Paho retries must be off or policy evidence is confounded.
Pre-test critique B: clockReady and zero jitter are fixtures and cannot establish NTP
or fleet desynchronization; synchronous test adapter cannot qualify nonblocking firmware.
Independent reviewer assigned current test bridge/runner and a separate full run.


CP10 result critique A: primary ordinary and sanitized real-broker runs passed all13;
independent tester reran13 and checked retry deadlines. No material scoped defect.
Result critique B: desktop adoption of initial socket and synchronous reconnect do
not implement Arduino nonblocking transport. NodeA recovery is not fleet qualification;
zero jitter is deterministic test input, not entropy. These gates remain open.


## CP11 five design decisions

1. Inject actual MqttClient; keep Arduino/Wi-Fi/TLS includes out of core.
2. Verify exact metadata and preserve existing inbox generation/bounds.
3. Streaming QoS1 writes confirm reply only after actual upstream endMessage success.
4. Exclusive callback ownership queues concurrent input during PUBACK waits.
5. Distinguish native upstream contract test from MCU, TLS and realtime validation.

Pre-test critique A found possible cross-scope publication; added matchesRoute before
publishing. Pre-test critique B (independent review) identified upstream poll discarding
incoming PUBLISH during endMessage; replaced polling-only design with owned callback.
Tests must cover concurrent PUBLISH/PUBACK before this checkpoint is finalized.


CP11 result critique A: independent actual-upstream171 assertions pass native and
ASan/UBSan; interleaved PUBLISH-before-PUBACK is preserved and not executed in callback.
Result critique B: shim may conceal actual Arduino allocation/time/SDK effects; no
board or hard realtime result inferred. Original source was unchanged after reviewer
hash capture. Complete core/host regression rerun passed after local file-mode repair.
