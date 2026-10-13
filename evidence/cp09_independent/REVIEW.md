# Independent checkpoint 09 review

Scope: `src/cts/ReconnectPolicy.h`, plus boundary review of the checkpoint 08
`Millis32.h`. Reviewed source hashes are recorded in REVIEWED_SHA256SUMS.
Production code was not changed by the reviewer. The independent test is
`tests/reconnect_review.cpp`.

## Actual outcome

- Native GCC C++11: **290 assertions, 0 failures**.
- AddressSanitizer + UndefinedBehaviorSanitizer, with
  `-fno-sanitize-recover=all`: **290 assertions, 0 failures**.
- No material production defect identified within this scope.
- Sanitizer emits environment warnings about executable-name symbolization;
  the complete output is retained. LeakSanitizer was disabled by the existing
  external `rebuild_work/native/no_lsan.cpp` environment helper because this
  sandbox does not expose the required `/proc` interface. Do not interpret this
  as a leak check.

## Independently checked failure paths

- No connection attempt without clock approval; clock loss cancels connecting
  or online state through a Close action.
- Exact timeout deadline rejects even a success callback; exact stability
  threshold resets the cap, while short online sessions preserve backoff.
- Stale connection and loss events cannot affect the new attempt, including
  stale timestamps that would otherwise trigger monotonic rollback.
- Duplicate success callback while Online is rejected.
- Stop/start does not reuse an attempt token.
- A relevant event or poll with backward monotonic time latches Fault.
- Invalid configuration and a retry deadline beyond uint64 range fail closed.
- Equal-jitter bounds and sample monotonicity are checked at six cap values,
  including 1, odd values, and UINT32_MAX; the minimum and maximum sample map
  to the inclusive minimum and maximum permitted delay.
- Millis32 handles a wrap from UINT32_MAX to zero, zero elapsed delta and
  maximum permitted delta; the exact half-range delta latches a fault.

## Limits and integration obligations

These tests use deterministic synthetic events/time and have no socket,
broker, Arduino SDK, MCU, sensor, thread, TLS implementation or entropy source.
They establish state-machine behavior for the exercised transitions; they are
not end-to-end reconnection or hardware validation and not a formal proof.
They do not exhaust all uint64 state combinations or test token exhaustion.

The adapter must obey the production contract: nonblocking connection attempts,
close/cancel on Close, scope late-event cleanup to the old socket, close the
transport before lost/stop, serialize access on one owner, supply a trustworthy
clockReady predicate and caller-owned jitter. A predictable or identical jitter
source across devices can still synchronize retries; no RNG is created here.

Millis32's documented sampling interval must be strictly less than 2^31 ms;
reconstruct it on reboot. It cannot detect missed complete wraps or certify UTC.

## Commands

```sh
g++ -std=c++11 -Wall -Wextra -Wpedantic -Werror \
  -I CodonTraceSwarm/src CodonTraceSwarm/tests/reconnect_review.cpp \
  -o reconnect_review
./reconnect_review
g++ -std=c++11 -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-sanitize-recover=all \
  -fno-omit-frame-pointer -I CodonTraceSwarm/src \
  CodonTraceSwarm/tests/reconnect_review.cpp \
  rebuild_work/native/no_lsan.cpp -o reconnect_review_sanitized
./reconnect_review_sanitized
```

The external no_lsan helper is environment-specific and is not a library
dependency; normal machines should run their sanitizer without that helper.
