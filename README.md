# CodonTraceSwarm — rebuild checkpoint 08

**An actual, buildable C++11 foundation, delivered as a complete cumulative snapshot.**

This is a newly tested rebuild after the previous workspace was lost. It is **not** a byte-identical recovery of version 0.1.0. API and test results belong to this snapshot only. Full Phase01 remains **PARTIAL** until remaining integration and completeness gates are closed. Local callbacks, registry-backed grants and host replies are now included; desktop broker execution is now tested; board execution is not.

Target: **all Arduino-supported board families**, with explicit memory profiles and
platform adapters. No board certification is implied; see [Arduino portability](docs/ARDUINO_PORTABILITY.md).

## Implemented

- Bounded, owned registry of up to eight immutable channel descriptors.
- Actual `genesis_swarm_v2` hello, reading and heartbeat JSON serialization.
- Scalar, vector, waveform, boolean and category samples.
- Strict UTF-8 validation, JSON escaping and bounded integer timestamps/sequences.
- Explicit invalid/null samples; genuine zero remains valid.
- Affine/table normalization checked for output range while transmitting source values.
- Whole-hello capacity gate and explicit serialization errors.
- Native behavioral tests and real pinned-host decoder/coordinator verification.
- Strict bounded command decoding, Unicode-aware duplicate rejection, scope/expiry and parameter validation; see [COMMANDS.md](docs/COMMANDS.md).

- Bounded eight-command FIFO, sixteen-entry deduplication history, clock-provider interface, deadline gates and local callbacks; see [DISPATCHER.md](docs/DISPATCHER.md).

- Registry-backed command grants and shared-sequence replies with a bounded outbox; see [COMMAND_SESSION.md](docs/COMMAND_SESSION.md).
- Actual host command → synthetic C++ callback → host reply-state integration test.

- Monitored clock state, explicit age/uncertainty, acquisition stamps and stale-sample gates; see [CLOCK_AND_ACQUISITION.md](docs/CLOCK_AND_ACQUISITION.md).
- [Target-plan progress and remaining requirements (فارسی)](docs/TARGET_PLAN_STATUS_FA.md).

- Bounded fragmented network-byte handoff with exact topic/session binding; see [NETWORK_INBOX.md](docs/NETWORK_INBOX.md).

- Real loopback TLS MQTT laboratory: two actual C++ nodes, broker ACLs and pinned Genesis host; see [BROKER_LAB.md](docs/BROKER_LAB.md).

- Arduino library metadata, board-selectable compile/report tool, two sketches and generic wrapping millis counter.

## Not included yet

Production board MQTT/TLS adapters, Wi-Fi connection management, board clock synchronization,
sensor hardware drivers, shadow/active policies, Arduino examples,
Arduino board-family compilation and physical hardware qualification. Command grants default to empty. Applications explicitly grant only installed operations
before begin; the hello and local policy use the same registry. This checkpoint is not deployable board firmware.

## Build and test

```sh
python3 tools/check.py
# Optional on a normal supported GCC/Clang environment:
python3 tools/check.py --sanitize
# Against the actual host revision documented below:
python3 tools/check.py --host /path/to/genesis
```

Or use CMake:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Host source must be at `74c7b3f15c2d093da03e392094814d63a4997c2c` (also the fetched remote main during this checkpoint). Original host source is **not** distributed here. The checker imports its real protocol and coordinator; it does not replace those with mocks. Fixture sensor values/time are explicitly synthetic. Same-input coordinator replay is not proof of physical validity or ALife engine replay.

## API

```cpp
#include <CodonTraceSwarm.h>
cts::Device device; // Use long-lived/global storage on constrained systems.
cts::ChannelSpec channel;
channel.name = "temperature";
channel.quantity = "temperature";
channel.unit = "degC";
channel.descriptor.featureId = "probe";
channel.descriptor.configId = "config1";
channel.descriptor.sourceUnit = "degC";
// Check every returned Error before proceeding.
auto defined = device.define(channel);
cts::SessionConfig session;
session.swarmId = "lab";
session.nodeId = "node1";
session.bootId = "unique_boot_nonce";
auto started = device.begin(session);
char wire[cts::TxBytes];
auto encoded = device.hello(wire, sizeof(wire));
// Deliver this hello before readings using your future transport implementation.
```

The snippet illustrates the API; it does not establish a connection or read a sensor.
`tests/native.cpp` is a runnable example of complete synthetic message production.

## Codec contract

Descriptors and session strings are copied; input buffers may be reused after successful
registration. The registry stays frozen after session start. Restart needs a new boot ID;
the caller must generate globally fresh nonces, not cycle between old IDs.

Only successful serialization consumes a sequence number. If a future transport retries
an envelope it must reuse the serialized bytes. The codec does not know delivery status
and does not enforce hello-before-reading. `wifi=true` declares eligibility for the
Wi-Fi-only host contract, not an observed network link. Ethernet-only applications must
not pretend to be Wi-Fi nodes.

Low-level telemetry UTC remains caller-supplied. TimedTelemetry now gates acquisition
stamps against MonitoredClock; the same monitor can gate command dispatch. A real
synchronized board adapter remains to be built.
Host receipt enforces telemetry freshness.
Source units stay on the wire; host normalization must not be applied twice. Tables never
extrapolate. User text byte limits are deliberately no wider than corresponding host
character limits. Numeric JSON formatting requires the C numeric locale; the writer
rejects locale output with a decimal comma rather than emitting malformed JSON.

See [VALIDATION.md](VALIDATION.md), [RESUME_FA.md](RESUME_FA.md),
[design decisions](notes/DESIGN_REVIEW.md), and [sources](notes/RESEARCH_LOG.md).

**License:** original library code MIT. No third-party dependency is bundled. Future
transport/driver dependencies need their own license inventory. No new-algorithm or
scientific-discovery claim is made by these interoperability tests.
