# Desktop TLS MQTT laboratory — checkpoint 07

This exercises real TCP/TLS sockets and a real MQTT broker with two compiled C++
CodonTraceSwarm nodes and the actual pinned Genesis coordinator. It is not an
ESP32 adapter, physical sensor experiment or scientific novelty result.
All lab clients connect as MQTT 3.1.1 (`mqtt.MQTTv311` in `tools/broker_lab/run.py`);
MQTT 5 is not exercised.

## Reproduce

Python 3.12, a C++11 compiler, OpenSSL executable and an available loopback port
are required. The host checkout must be at
`74c7b3f15c2d093da03e392094814d63a4997c2c` from
https://github.com/Parvaz-Jamei/codontrace-genesis .

```sh
python3 -m venv .venv-broker
. .venv-broker/bin/activate
python -m pip install -r tools/broker_lab/requirements.lock
python tools/check.py --host /path/to/genesis --build-dir build-native
python tools/broker_lab/run.py --host /path/to/genesis \
  --node build-native/cts_broker_node --output build-broker-evidence
```

Do not run Python with `-O`: this test harness uses assertions. The native checker
builds the lab executable but does not automatically run the broker lab. Hosted CI
and the CMake frontend were not executed in this environment.

## Tested scenarios

1. Untrusted certificate authority is rejected.
2. A trusted certificate with the wrong hostname is rejected.
3. Wrong password results in disconnect, without a successful connection.
4. Anonymous login results in disconnect.
5. Two authenticated nodes subscribe at QoS1 with verified TLS.
6. A node cannot subscribe to another node's command topic.
7. A node cannot publish on another node's telemetry topic.
8. Actual C++ hello travels through TLS to the actual host.
9. Host commands produce success and deliberate callback failure; repeating a
   command does not repeat execution. Replies reach host acknowledged/rejected state.
10. An actual broker-retained command is rejected by the C++ inbox.
11. Controlled broker restart, reconnection, fresh hello with the same boot and
    advancing sequence, and a subsequent successful command.

The restart closes clients first: it does not simulate an abrupt interruption
while a publication is in flight. Application duplicates are tested, not forced
MQTT transport DUP flags or durable broker/session recovery.

## Architecture and boundaries

Paho transports bytes to/from each C++ subprocess. The C++ node uses Device,
CommandSession and NetworkInbox, feeding input in 17-byte fragments. Paho's
publish completion/PUBACK precedes the test adapter's reply confirmation. PUBACK
alone does not establish application acceptance; the test separately asserts the
actual coordinator state. Python does not fabricate the C++ reply/reading envelopes.

The subprocess line protocol is a controlled test interface, not a production
untrusted-input transport. Authentication metadata is supplied after the Python
TLS client authenticates. The sensor is a synthetic constant 42.5 and the injected
application clock is fixed; certificate verification uses the real OS clock.
The hello's Wi-Fi flag belongs to the synthetic fixture and is not proof of Wi-Fi hardware.

Broker listens only on 127.0.0.1. Each run creates ephemeral CA/server credentials
and random distinct passwords, removed on exit. Hostname checking remains enabled;
TLS minimum is 1.2 (this recorded run negotiated 1.3). Explicit file authentication
and separate publish/subscribe ACLs are enabled, without anonymous authentication.
Secrets, dependencies and executables are not bundled. Version pins are provided,
not a hash-locked/hermetic supply-chain claim.

Expected rejected TLS/authentication attempts may produce broker connection-reset
tracebacks. These diagnostics are retained in evidence; they are distinct from a
failed scenario. C++ sanitizer diagnostics are checked before recording success.

## Evidence and remaining work

See `evidence/broker_lab/results.json`, `live.log`, `broker.log` and `VALIDATION.md`.
Final live execution used ASan/UBSan nodes with non-recovering UBSan. LeakSanitizer
was disabled solely for this sandbox's missing /proc support; symbolization warnings
are preserved. Native ordinary suites were separately rerun. Independent CP07 review
is PENDING because the assigned tester exhausted its quota.

Next: ESP32 time and Wi-Fi/TLS adapters, task-safe handoff, reconnect/backoff,
abrupt loss/in-flight publish tests, memory maps, firmware builds and hardware soak.

## Primary technical references

- Paho client API: https://eclipse.dev/paho/files/paho.mqtt.python/html/client.html
- aMQTT broker configuration: https://amqtt.readthedocs.io/en/latest/references/broker_config/
- Installed aMQTT 0.12.1 plugin configuration/source was inspected for the actual
  authentication and ACL fields used; do not substitute fields from another release.

This lab validates protocol integration, not ALife hypotheses or sensor accuracy.


## Checkpoint 10 extension: policy-driven desktop recovery

The current runner additionally executes scenarios 12 and 13: abrupt process kill
with active clients; actual node disconnect callback; two real connection refusals
while the broker stays down; restart; a new TLS/MQTT/subscription attempt triggered
by the C++ ReconnectPolicy; actual fresh hello and host command/reply acceptance.
Paho automatic reconnect is disabled. All existing eleven scenarios are retained.

The initial policy attempt is attached to an already-established socket so the lab
can enter Online; subsequent three attempts are actual new connection attempts.
The C++ node runs the real policy and dispatches its loss/recovery to the real inbox.
Python supplies OS monotonic milliseconds and drives the socket operations in response
to decisions. The trace records attempt token, due time, observed time, counts and
hello. Expected counts are four policy attempts, three failures, two socket refusals.

Scope: a desktop integration experiment, not a deployable Arduino backend. Paho connect
calls are synchronous on loopback, with connect_timeout configured; this is not evidence
of a robust asynchronous firmware timeout/cancellation implementation. Jitter input is
fixed zero for reproducibility, not a fleet entropy evaluation. The application clock
approval is a fixture; TLS still verifies the actual CA/hostname against the OS clock.
The broker is killed while clients are connected but no publication is deliberately
held in flight. No exactly-once network delivery or persistent-session recovery claim.

Current evidence is evidence/broker_lab_cp10; earlier evidence/broker_lab remains the
original CP07 run. Test output directory reports are cleared at start so an interrupted
or failing rerun cannot retain an earlier successful results.json.

Technical reference checked: https://eclipse.dev/paho/files/paho.mqtt.python/html/client.html
(connect_timeout, reconnect_on_failure and callbacks; actual dependency stays Paho2.1.0).
