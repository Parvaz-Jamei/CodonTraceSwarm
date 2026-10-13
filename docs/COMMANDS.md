# Bounded command decoding (introduced in checkpoint 02)

`#include <cts/Command.h>` exposes `CommandDecoder`, `CommandContext`, owned `Command`,
error codes and a separate `validateChannel()` gate. The decoder has 4096 input bytes,
96 tokens, nesting depth 8, bounded string scratch and no heap allocation. Its native
object size is 6,456 bytes on the tested native target. Use static/long-lived storage on small targets.

```cpp
cts::CommandDecoder decoder;
cts::CommandContext context;
context.swarmId = "lab";
context.nodeId = "node1";
context.bootId = "fresh_boot_nonce";
context.nowMs = /* actual synchronized UTC milliseconds */ 0;
cts::Command result;
auto error = decoder.decode(bytes, byteCount, context, result);
// context.nowMs=0 deliberately rejects until a real clock is supplied.
// Only use result when error==cts::CommandError::Ok.
```

This is API pseudocode; bytes/byteCount must come from a bounded receive path.

## Validation performed

- Strict JSON grammar, complete consumption, UTF-8 validity and escape decoding.
- Duplicate object keys rejected after escape decoding, including unknown/nested keys.
- UTF-16 surrogate pairs decoded; lone surrogates and embedded NUL are rejected.
- Exact envelope and operation-specific keys; no silent extra-field acceptance.
- Exact schema, pending status and supplied swarm/node/boot scope.
- Integer timestamps ≤2^53−1; creation not after current UTC; expiry after now;
  lifetime positive and at most 60 seconds. Time error is explicit, never clamped.
- All seven current host command shapes: sample/raw/features/result, set_rate,
  pipeline and capsule. Finite numeric values and actual host bounds are enforced.
- Lowercase 64-hex optional source digest; this label is not a signature.
- Decoded identifiers are ASCII and at most 63 bytes, matching this library profile.

`validateChannel(decoded, registeredSpec)` checks channel identity, declared rate
bounds and scalar-only pipeline/capsule use. Pass only a successfully decoded command
and the validated registered descriptor. It is not a substitute for operation authorization.

The result owns its strings. Every failure resets the output so a stale successful
command cannot accidentally be reused. Decoder scratch is overwritten on the next call;
no returned pointers refer to it. One task must own a decoder at a time.

## Explicit remaining layers

A successful decode **does not execute or advertise the operation**. Default hello
advertises `operations:[]`; explicit installed grants are supported in checkpoint04. The controller fixture used by tests advertises seven
shapes only to generate real host messages; it is labeled synthetic, not deployed firmware.

The transport must still enforce authenticated broker/topic scope and retained-message
policy. The local dispatcher now supplies queue, deduplication and execution-time gates;
see [DISPATCHER.md](DISPATCHER.md). Its policy must connect to the device registry.
CommandSession now supplies host replies; network authentication remains pending. This parser's earlier Ok
is not a permanent authorization.

No physical sensor driver, rate scheduler, filter installation or remote code execution
is supplied. The local dispatcher invokes an application-provided callback. `capsule` is the host's three-parameter scalar filter schema, not a
neural-model or binary firmware container. No exactly-once or hardware claim is made.
