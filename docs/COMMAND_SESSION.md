# Registry-backed commands and host replies — checkpoint 04

`cts/CommandSession.h` adds a local orchestration layer over the actual Device,
RegistryPolicy and Dispatcher. `tests/session_probe.cpp` is the runnable **synthetic**
example; `tools/verify_session.py` connects it to the unmodified real host coordinator
via subprocess pipes. This is not an MQTT or hardware test.

## One registry and explicit capabilities

`Device::define(spec)` owns the validated descriptor and rate/type constraints.
`Device::allow(channel, operation)` is called only for an operation the application's
executor actually implements. It must be called before begin. Default grants are empty;
unknown channels/enums and non-scalar pipeline/capsule grants are rejected.

`hello.operations` is the unique union of explicit per-channel grants. The wire contract
advertises per-node operations, but `RegistryPolicy` additionally gates the individual
channel. No descriptor shadow copy or manually synchronized second policy table exists.
`SetRate` checks the stored rate interval. Grants and descriptors freeze at begin.

Declaration is not implementation: allowing an operation does not supply its scheduler,
filter, hardware driver or model. The only end-to-end example grants sample, implemented
by a synthetic executor that serializes a known constant reading. Seven-operation
parser interoperability remains a separate test, with a labeled controller fixture.

## Integration sequence

1. Define channels and grant only installed operations.
2. Construct CommandSession with a Device, ClockProvider and CommandExecutor that
   outlive the session. Use one session per Device and one event loop, not two tasks.
3. Call session.begin(config), which starts the Device and Dispatcher with matching
   owned swarm/node/boot identifiers. Do not separately start Device first.
4. Serialize and publish hello before any subsequent envelopes. The future transport
   must preserve the single Device sequence across hello, readings, heartbeats and replies.
5. Feed bounded authenticated command bytes to session.submit. Inspect BOTH
   SessionResult.code and .dispatch.code: session Ok alone does not mean admission.
   DeliveryContext flags remain trusted adapter assertions, not implemented authentication.
6. Call poll. Only a Completed dispatcher event generates a reply. Queued never means
   success. Succeeded maps to ok=true; Failed/Expired/Denied completion maps to false.
7. While ReplyPending, publish replyData()/replySize() in order. Retry EXACTLY those
   bytes. poll returns the same pending data and does not execute another callback.
8. After the exact bytes are accepted by your ordered transport, call
   confirmReplyPublished(replySequence()). A mismatched/stale sequence cannot release
   a newer pending reply. This method does NOT prove host acceptance.

A pending reply locks NEW Device serialization (Busy) until release. This avoids a
later reading overtaking its sequence. Frames serialized earlier, including readings
produced inside the executor, still MUST be published first by the transport adapter.
The outbox is one fixed 1,024-byte buffer. Dispatcher admissions can continue until its
bounded queue fills, but another callback cannot execute until the reply is released.

If reply serialization fails, completion is retained and poll retries serialization,
not execution. A sequence-exhausted codec cannot recover in the same boot; do not clear
that error and claim delivery. CommandSession retains state only in RAM.

## Duplicate delivery and status boundaries

The same accepted command cannot invoke the callback twice within the dispatcher's
retention guarantees. Its duplicate reports cached state and does not generate a new
reply sequence. If an original reply remains pending, retransmit the existing bytes.
After confirming publication, this checkpoint does not retain reply bytes for resend.
Premature confirmation can lose a reply: broker/transport integration must define this
boundary correctly. Exactly-once physical action and durable delivery are not claimed.

Rejected admission (bad wire, scope, policy, capacity or authentication) currently returns
a local error without a wire reply. Only queued commands that reach Completed produce
one. The caller must surface admission failures; do not pretend the host received them.

A command can succeed locally while its reply arrives too late for the host. The pinned
host expires commands before ingest and refuses replies to non-pending commands. The
integration test explicitly verifies this disagreement: local true does not override
host expired. Do not relabel it as acknowledged or retry the physical action.

External end/restart of Device invalidates the old CommandSession. Do not destroy a
session with an unsent reply; destruction abandons it and releases the serialization
lock. A new session needs a fresh boot ID. No persistent recovery is implemented.

## Lower-level API

`Device::reply(commandId, ok, optionalMessage, output, capacity)` serializes the actual
`genesis_swarm_v2` reply schema and shares the Device sequence. It validates identifiers,
UTF-8 and a 256-byte message limit. It is a codec primitive, not evidence of completion;
prefer CommandSession for mapping actual dispatcher outcomes. Always check Error.

`Device::commandAllowed()` expects decoded operation/parameters when used as authorization;
RegistryPolicy is called only after the strict parser in the standard session flow.

## Verified scope

- 196 native session assertions, including pending-byte identity, outbound lock,
  stale confirmations, queue wait expiry, scope change, callback reentry and stored rates.
- Actual host hello -> generated sample commands -> C++ callbacks/readings/replies ->
  host acknowledged/rejected states. No rewriting of the hello capability list.
- Separate negative test: positive result arriving at host expiry is rejected.
- Synthetic sensor value 42.5; no measured accuracy, sensor calibration or novelty claim.
- Measured x86-64 sizeof(Device)=34,928; sizeof(CommandSession)=12,352 (includes dispatcher).
  Keep both long-lived on constrained targets. Sizes exclude TLS, callback stack and
  transport buffers and are not MCU firmware measurements.

## Sources and design evidence

Pinned host: 74c7b3f15c2d093da03e392094814d63a4997c2c, swarm/protocol.py reply validation
and swarm/runtime.py command/reply state transitions. Original host source is not bundled.

MQTT 5.0 section 4.10 distinguishes application request/response from transport delivery:
https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html
Consulted 2026-10-13 Asia/Tehran. This package uses the existing Genesis command_id as
application correlation; it does not yet implement MQTT Response Topic properties.
