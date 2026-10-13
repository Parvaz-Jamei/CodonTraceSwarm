# Portable reconnect policy — checkpoint 09

`cts::ReconnectPolicy` is board-neutral, single-owner and allocation-free. It
produces decisions; it does not pretend to implement Wi-Fi, MQTT, TLS or a clock.
Any Arduino backend can consume it if it satisfies the contract below.

## Adapter lifecycle

1. On a new application boot create the policy and a truthful time source. Call
   start with a monotonic timestamp. Preserve the MQTT client ID and boot ID over
   reconnects; use a new boot ID for a real application reboot.
2. Call poll(now, clockReady, jitter32) from the single owning event loop. clockReady
   must reflect validated UTC suitable for certificate verification, not millis readiness.
3. On Connect, retain the returned token with that socket/attempt and initiate
   nonblocking connection with real CA/hostname checks and application authentication.
   Only after TLS, MQTT authentication and required subscription setup succeed call
   connected(token,now,clockReady,jitter). On true, call NetworkInbox.connected(token)
   and send a fresh actual Device hello before readings. Never reset the Device sequence.
4. On Close, cancel/close the corresponding attempt and call NetworkInbox.disconnected.
   Drop partial and queued received frames through its documented loss accounting.
5. For an unexpected disconnect, first close transport, then call lost(token,now,jitter)
   and NetworkInbox.disconnected only for the current accepted generation. Ignore old
   generation callbacks; never let an old callback close the new socket.
6. A false connected result means that attempt must not be used; close its socket.
   A monotonic fault latches. Stop does not close sockets itself; the adapter closes
   transport/inbox first. Start after stop preserves attempt token uniqueness.

Board SDK callbacks must hand off events safely to this loop. The policy does not
provide a mutex. A blocking connect inside poll's caller defeats the timeout contract:
use SDK asynchronous connection or a separately controlled worker with bounded cancellation.
No command/publish replay is performed by this policy; CommandSession remains responsible
for command effect deduplication and pending reply semantics.

## Retry and timing rules

First attempt is immediate once the clock gate is ready. Subsequent retry windows
start at initialMs and double up to maximumMs. The selected delay lies between
ceil(window/2) and window using caller-supplied 32-bit jitter. This has a positive
minimum and no sleep inside the policy. Provide independent device entropy or a
logged seeded test source; a constant argument is valid for deterministic testing
but does not desynchronize a fleet. No new RNG implementation is hidden inside.

Failures during a flapping connection continue backing off. The cap resets only
after stableMs online. Each attempt has timeoutMs; completion at the deadline is
rejected. No connection begins while the clock gate is false, and clock loss closes
an active attempt/connection. Timer addition overflow and observed monotonic rollback
latch Fault. Counters saturate rather than wrapping. Token exhaustion also faults.

Defaults (1s base, 60s cap, 10s timeout, 30s stable) are configurable engineering
starting points, not hardware-derived optimal values. Measure handshake latency,
resource cost and outage behavior on each target before product tuning.

## Verified scope

Native regression connects the policy's generations/loss events to the actual
NetworkInbox, including a partial message lost on disconnect and rejection of
old-generation fragments after reconnect. Transport events here are SYNTHETIC.
The CP10 desktop TLS broker lab now drives this policy through actual broker loss
and recovery. No automated reconnect-over-Wi-Fi firmware claim or Phase02 completion follows.
Independent review details are recorded separately in evidence/cp09_independent.

Primary source: https://aws.amazon.com/builders-library/timeouts-retries-and-backoff-with-jitter/
Background on retry pressure and jitter informs this engineering design; no novel
algorithm or scientifically demonstrated performance improvement is claimed.
