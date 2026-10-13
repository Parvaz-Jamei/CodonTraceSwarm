# Independent checkpoint 11 review

Scope: optional ArduinoMqttAdapter with the **real ArduinoMqttClient 0.1.8
MqttClient.cpp**, pinned at `4699dda841c08b01036cd14412330bddc6c94edb`.
The upstream source stays external. The reviewer supplied test-only native
Arduino/String/Client facades and a synthetic byte-stream peer. Production
adapter fixes were authored by the primary implementation agent.

Final native execution: **171 assertions, 0 failures**. Final AddressSanitizer +
UndefinedBehaviorSanitizer execution with abort-on-error: **171 assertions,
0 failures**. No additional material adapter defect was identified in this scope
after the callback correction. Sanitizer symbolization warnings are preserved.

## Material finding and correction

The initial polling design could discard an incoming command during a reply's
PUBACK wait. Actual upstream `endMessage()` calls `poll()` repeatedly, and
`poll()` discards unread PUBLISH payload unless an onMessage callback consumes
it. Simply avoiding parseMessage mid-frame was insufficient.

The adapter was changed to exclusive callback ownership. The callback reads
the complete bounded command into NetworkInbox without dispatching commands.
The independent regression injects a real encoded PUBLISH **before PUBACK** in
the synthetic stream. The actual upstream parser invokes the adapter callback,
the reply completes, the command remains queued, and its executor runs only
after an explicit inbox drain plus session poll. This regression passes.

## Other independently exercised behavior

- A fixed MQTT header split across two receive calls keeps parser state.
- Payload bytes arriving later during upstream yield are consumed correctly.
- Short payload reads close the transport; no padded or partial command enters
  the queue. Retained, wrong-topic, QoS0, empty, and oversized commands fail closed.
- Exact reply bytes pass through the real upstream streaming publish path.
- Missing PUBACK, wrong packet-id PUBACK, header short-write, and body
  short-write preserve the application's pending reply and close transport.
- Foreign-session routing is rejected.
- A second adapter cannot take an already-owned callback slot; failed ownership
  acquisition does not unregister the existing owner. The active owner's
  destructor closes the transport and releases its slot.
- False security attestation and backward receive timestamps fail closed.

## Limits

This is **not an Arduino board compile or physical board test**. Native shims
do not validate real String allocation, SDK threading, Client implementations,
TLS, certificate checking, Wi-Fi, MQTT broker authentication or real network
timing. The synthetic peer returns valid MQTT wire responses but is not a
network service. No claim is made that all Arduino boards fit the library.

The caller still supplies the security attestation and subscription. Exclusive
callback/client ownership and single-threaded use are required. Multiple client
instances must use distinct adapter Slot template parameters. This test uses
the default upstream C-function callback API, not its optional std::function API.

**Blocking limitation:** upstream `clientTimedRead()` permits a 1000 ms wait per
byte; its connection timeout does not impose a total payload deadline. A slow
peer can cause long callback occupancy. This synchronous adapter is unsuitable
for hard realtime control or a claim of bounded whole-message latency. Use a
separately engineered asynchronous backend for those requirements.

Upstream uses GNU variable-length stack arrays, so the native verification uses
`-std=gnu++11`; no strict ISO C++/pedantic upstream-compliance claim is made.

## Reproduction

```sh
python3 tools/check_arduino_mqtt.py \
  --upstream /path/to/clean/pinned/ArduinoMqttClient \
  --build-dir build-arduino-mqtt
python3 tools/check_arduino_mqtt.py \
  --upstream /path/to/clean/pinned/ArduinoMqttClient \
  --build-dir build-arduino-mqtt-sanitized --sanitize
```

The runner verifies the exact upstream commit and rejects dirty src files.
It does not fetch dependencies or contact a network. Sanitized validation in
this sandbox used the existing external no_lsan helper because of unavailable
`/proc`; this does not qualify as leak testing. Complete logs and current
reviewed source hashes accompany this report.
