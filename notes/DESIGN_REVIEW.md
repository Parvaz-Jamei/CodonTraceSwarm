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
