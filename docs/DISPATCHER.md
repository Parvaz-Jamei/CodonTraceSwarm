# Local command lifecycle — checkpoint 03

`cts/Dispatcher.h` connects the existing strict decoder to a bounded FIFO and a
synchronous application callback. This is a local execution primitive, not a
network-connected sensor driver. The `Device` codec defaults to no operations; checkpoint04 adds explicit grants and
[CommandSession](COMMAND_SESSION.md) for registry-backed policy and replies.

## Integration contract

1. Register/validate channels with `Device` and retain an immutable application registry.
2. Implement `ClockProvider::read()` with one coherent synchronized UTC/64-bit
   monotonic millisecond snapshot. Report `synchronized=false` whenever time is not
   trustworthy. No SNTP service, clock accuracy estimator or board timer adapter is
   supplied in this checkpoint. Do not pass an unextended wrapping 32-bit `millis()`.
3. Implement `CommandPolicy::permits()` using both an explicit implemented-operation
   allowlist and `validateChannel(command, validatedSpec)`. Deny unknown channels.
   Policy callbacks must be bounded and side-effect-free in production.
4. Construct one long-lived Dispatcher per boot with clock and policy references
   that outlive it; call `begin` once with the SAME scope as the Device session.
   It copies the scope. There is no reset method that silently forgets history.
5. A future authenticated transport adapter must verify broker/topic/identity scope
   and provide truthful `DeliveryContext` flags. The default flags reject delivery.
   `authenticated=true` is an adapter assertion, NOT cryptographic verification.
   Retained command messages are rejected. No MQTT integration is shipped here.
6. Submit wire bytes, inspect result, and poll `runOne(executor)` in one event loop.
   All mutation, including clock/policy callbacks, is single-threaded. Reentrant
   submit/runOne/begin calls return Busy; this is NOT a cross-thread mutex.
7. Executor callbacks are synchronous and `noexcept`. Both successful and failed
   callbacks become terminal; a failed callback is never implicitly retried.
   Return values report local lifecycle only. The low-level dispatcher does not serialize ACKs; CommandSession now adds wire replies.
   Persistence and network delivery remain future work. Never map Queued to acknowledged success.

## Bounds and failure behavior

- FIFO holds eight commands; shared history holds sixteen queued/terminal entries.
- Admission checks schema, scope, timestamps, parameters, policy and capacity.
- An identical decoded command ID/content returns Duplicate and its cached state.
  Same live ID with different semantic content returns Conflict; whitespace and
  JSON member order do not change identity. Bytewise struct padding is not compared.
- QueueFull / HistoryFull reject new admission without dropping accepted work.
  Caller must back off; no hidden unbounded backlog exists.
- Terminal history can be reclaimed only after BOTH UTC expiry and its monotonic
  deadline. Live entries are not evicted for traffic. Expired queued commands are
  drained by runOne, emitting Expired without a callback; polling remains required.
- At execution, policy is re-evaluated and time is sampled again AFTER that policy
  check. A command expires if either deadline has been reached. A timer overflow,
  observed UTC reversal or observed monotonic reversal fails closed.
- ClockUnavailable leaves queued work intact and can recover when a valid clock
  returns. ClockFault is latched for the object: recover through a genuinely new
  boot/session with a fresh unique boot ID, not by recycling the old session.
- UTC jumps forward can expire work early. This deliberately conservative policy
  trades availability for avoiding execution outside the permitted interval.
- Deadline gates START of execution. A slow callback is not interrupted at expiry;
  application code must bound its duration and handle physical side effects safely.

## Exact limits of deduplication

At-most-once callback invocation is tested for an accepted command while its record
is retained in the same live Dispatcher. This is NOT a durable exactly-once physical
operation guarantee. Reboot, power loss, a second Dispatcher with the same boot ID,
external retries with new IDs, or a newly timestamped command after history expiry
require application-level treatment. Original expired wire commands remain rejected.
No persistent journal, compensating action or transactional actuator is implemented.
Clock validity, monotonic progression and network authentication depend on adapters.

## Footprint and runnable verification

Measured native x86-64 object sizes: Dispatcher 11,000 bytes (including its decoder),
Device 34,920 bytes (checkpoint04). Allocate long-lived storage on constrained targets; do not put
these objects on a small task stack. These are not ESP32 firmware RAM measurements
and exclude TLS, stack and transport buffers. The dispatcher itself does not allocate
heap memory. Host test fixtures use std::string and ordinary dynamic memory.

Run `python3 tools/check.py`. `tests/dispatcher.cpp` exercises callbacks, queue wrap,
backpressure, cache retention, mutable policy, clock faults, untrusted/retained input,
input ownership, callback failure and reentry. Its clock and executors are explicitly
synthetic. The checkpoint04 session integration additionally verifies host acknowledgment with a
synthetic callback; no hardware execution is claimed.

## Primary-source rationale

MQTT QoS 1 permits duplicate delivery; application command identity must therefore
be handled independently of transport packet flags. See OASIS MQTT 5.0 sections 4.3
and 3.3.2.3.3:
https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html

ESP Timer distinguishes elapsed time from calendar time, and deep sleep restarts
its timebase. Future board adapters must account for that lifecycle:
https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/esp_timer.html

Consulted 2026-10-13 Asia/Tehran. These sources motivate engineering design; neither
source establishes a new algorithm or scientific discovery for this implementation.
