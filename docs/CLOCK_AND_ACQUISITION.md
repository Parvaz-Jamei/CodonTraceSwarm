# Monitored time and acquisition stamps — checkpoint 05

This adds actual portable validation logic, not a board SNTP client. `MonitoredClock`
implements the existing ClockProvider interface used by CommandSession. `TimedTelemetry`
uses the same monitor to gate sample serialization. Device::reading remains a low-level
codec API; applications that bypass the gate must enforce equivalent time rules themselves.

## TimeSource contract

A board adapter implements `TimeSource::observe()` as a coherent, non-reentrant snapshot:
UTC epoch milliseconds, 64-bit monotonic milliseconds, synchronization status, age since
synchronization and an uncertainty estimate with explicit known/unknown flags. It must
not mutate Device/session objects. Source and clock objects outlive their consumers.

No invented uncertainty is allowed. The monitor validates supplied metadata; it cannot
independently measure clock accuracy or authenticate a time server. Millisecond
quantization, network delay/asymmetry and oscillator drift belong in the adapter's
error budget. Sync success by itself is not an estimate of all these errors.

Default configurable engineering policy:

| Limit | Default | Boundary |
|---|---|---|
| Maximum synchronization age | 300,000 ms | equality allowed |
| Maximum known uncertainty | 250 ms | equality allowed |
| Maximum observed UTC/monotonic delta discrepancy | 50 ms | equality allowed |
| Unknown uncertainty | rejected | explicit opt-out available |
| Maximum sample age | 3,000 ms | equality allowed |

These numbers are configuration defaults, not empirically validated universal bounds.
Choose age/skew budgets compatible with host TTL and the sensor task. If unknown
uncertainty is explicitly allowed, health keeps `uncertaintyKnown=false`; it never turns
unknown into zero. Known excessive uncertainty is still rejected. Sync age must be known.

## Clock state machine

- Unsynchronized, invalid UTC, unknown/stale age and unknown/excessive uncertainty yield
  synchronized=false and UTC zero. Recovery is possible with valid new observations.
- Monotonic rollback is checked on EVERY observation, including unsynchronized input.
- New trusted UTC is compared against the last trusted mapping, including across an
  outage. UTC rollback or a delta discrepancy beyond the policy limit latches a fault.
- A latched fault has no reset/clamp API. Recover with a new monitor and genuinely new
  measurement/command boot session. Do not rebuild the monitor while preserving old
  samples and a live boot identity.
- health() is the last observation, not a fresh sample. Counters saturate; observations,
  accepted, rejected and latched faults are separate. Invalid UTC never leaks as ready.

This is an observed-discontinuity gate, not a frequency estimator or an NTP filter.
Small consistent drift may remain within per-observation limits; truthful sync age and
uncertainty metadata are still essential. A correction between reads can be missed if
it is undone before observation. No continuous monitoring guarantee is implied.

ESP32 adapters should combine the platform epoch clock with a boot-scoped extended
monotonic timer. An unextended 32-bit millis() wrap is not supported. Deep sleep/timebase
restart requires a fresh boot/session. TLS bootstrap time must not depend on first
connecting to the broker. Board adapter, Wi-Fi/NTP and TLS integration remain pending.

## Acquisition stamps and outbound data

1. Define/start Device (or use CommandSession.begin).
2. Construct TimedTelemetry(device, monitoredClock, maxAgeMs).
3. At acquisition START, call capture(stamp) and check TelemetryCode::Ok. For waveform
   windows this is the window start, not its final sample or later transmission time.
4. Build a Sample with utcMs equal to stamp.utcMs(). A valid zero stays zero; driver
   failure is Sample::invalid(reason, stamp.utcMs()), not an invented measurement.
5. Later call telemetry.reading(channel, sample, stamp, output, capacity).

The stamp owns its numeric fields and binds to the originating TimedTelemetry instance
and Device session counter. It is not a credential; deliberate C++ memory corruption is
outside the contract. The originating gate/clock/device must remain alive at their same
addresses while stamps are in use. Stamps from an earlier Device session are rejected.

reading rechecks clock validity and age in BOTH UTC and monotonic time. Mismatched UTC,
foreign stamp, stale sample or clock failure cannot consume a sequence number. The
original acquisition timestamp is never relabeled as send time. Codec errors (including
CommandSession's pending-reply Busy) are reported separately; retry only while fresh.
Repeated encoding of a still-valid stamp is permitted; this is not a sample deduplication
cache. For transport retry, reuse already encoded bytes rather than encode again.

This gate does not schedule sensors, buffer streams, delete queues, synchronize channels,
or serialize clock diagnostics in the existing heartbeat (which remains {}). Export its
health through future legal diagnostic channels/UI, without changing the host schema.
A high-rate phase-alignment claim needs a separate accuracy study and finer timing data.

## Executed validation and sources

`tests/clock.cpp`: 1,101 assertions including 1,000 synthetic sequential observations.
`tests/clock_review.cpp`: 50 independently authored boundary/adversarial checks.
`clock_probe.cpp` -> `verify_clock.py`: actual pinned-host acceptance of delayed
acquisition UTC, valid zero and invalid null; stale sample rejected before serialization.
These are native synthetic tests, not NTP or MCU measurements.

Primary references consulted 2026-10-13 Asia/Tehran:
- Espressif System Time (instant/smooth corrections and synchronization callbacks):
  https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32/api-reference/system/system_time.html
- Espressif ESP Timer (elapsed time, light/deep sleep lifecycle):
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/esp_timer.html
- User-supplied plan: contract/SESSION_COMMANDS.md and phases/PHASE_02.md.

No source claims that the chosen defaults establish scientific timing accuracy; this
implementation supplies explicit gates and traceable failures for later validation.
