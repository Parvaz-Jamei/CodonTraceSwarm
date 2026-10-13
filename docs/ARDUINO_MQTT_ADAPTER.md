# Optional ArduinoMqttClient adapter — checkpoint 11

The new `cts::ArduinoMqttAdapter<MqttClient, Slot>` connects the actual upstream
ArduinoMqttClient API to NetworkInbox and CommandSession. It is an optional
synchronous backend component, not a complete board application. The core remains
independent of ESP32 or any vendor SDK. It does not establish Wi-Fi or TLS itself.

## Exact dependency and license boundary

Reviewed external dependency: ArduinoMqttClient 0.1.8, Git SHA
`4699dda841c08b01036cd14412330bddc6c94edb`, from
https://github.com/arduino-libraries/ArduinoMqttClient .
Upstream source headers and LICENSE.txt identify LGPL-2.1-or-later. Dependency source
is not copied into the MIT library package; install it separately and preserve its
license/relinking obligations when distributing linked firmware. Arduino runtime
shims under tests are original test scaffolding, not a board core or TLS stack.

Use the default function-pointer callback API: do NOT define
MQTT_CLIENT_STD_FUNCTION_CALLBACK. Template instantiation is opt-in, so projects that
only use the core do not need this dependency. No implicit dependencies or hidden
Wi-Fi SDK includes have been added to the portable library.

## Wiring contract

```cpp
#include <ArduinoMqttClient.h>
#include <cts/ArduinoMqttAdapter.h>
// secureClient is supplied/configured by the board application.
// NetworkInbox must first be bound to this Device's exact swarm/node scope.
// MqttClient mqtt(secureClient);
// cts::ArduinoMqttAdapter<MqttClient, 0> adapter(mqtt, inbox);
```

Before attach, the application must verify its TLS CA/hostname and actual certificate
clock, establish authenticated MQTT, set a stable client ID, and successfully subscribe
to the bound down topic at QoS1. Then call `attach(connectionGeneration, true)`.
That boolean is a security attestation from the application, not a TLS verifier.
Passing true for an insecure Client does not make it secure. The caller owns capability
truthfulness, boot identity, reconnect policy, Wi-Fi and the board time source.

The adapter installs and exclusively owns `mqtt.onMessage`. While attached, never
replace that callback, call parseMessage/read/poll elsewhere, or give the same mqtt
object to a second adapter. One attached instance per `<Mqtt, Slot>` is allowed;
use distinct Slot values and distinct MQTT objects for independent connections.
A competing same-slot attach returns false without replacing the first owner.
The client and inbox must outlive the adapter. Destructor/close unregister the callback.
One serialized event loop only; no ISR or unsynchronized SDK task access.

Call receive(monotonicMs) to service MQTT. Callback validates exact topic, retained=false,
QoS1 and payload length <= CommandBytes, copies in chunks of128, and queues into the
existing inbox. Callback never parses a command, runs an executor or publishes a reply.
After receive returns, the caller may invoke inbox.pollOne(session), then session.poll.
Use publishReply(session) only after all preceding reading/hello messages are sent in
shared-sequence order. It selects exact up topic from the bound inbox, rejects a foreign
session, uses streaming size/retain=false/QoS1, checks complete write and endMessage,
and confirms session reply only after upstream PUBACK success. PUBACK is NOT host acceptance.

On transport/security/clock loss, call close and report loss to ReconnectPolicy. The
adapter stops MQTT, releases its callback and drops the inbox via existing loss counters.
A fresh generation and actual hello are required after reconnection. Pending reply/hello
ordering across reconnect is still a separate integration gate; do not discard pending
replies merely to make hello fit. No full automatic reconnect coordinator is included.

## Why callback ownership matters

Inspection and independent testing exposed a real upstream interaction: endMessage
loops over poll while waiting for PUBACK. Without a receive callback, an incoming
PUBLISH can be discarded by a later poll. The initial polling-only adapter design was
replaced. The callback now queues a concurrent command even during a reply PUBACK wait;
command execution remains outside that callback. This is covered using real upstream
MqttClient.cpp with a controlled byte-stream transport, not a substitute MqttClient.

## Failure behavior and limits

Oversized, empty, retained, wrong-topic, non-QoS1, short-read or queue-full input closes
the transport. Partial payload is never zero-filled or admitted as complete. Invalid
input can drop previously queued frames through inbox.disconnect accounting. Short
writes or missing PUBACK retain the pending CommandSession reply and close the link.
The caller must observe the return code and reconnect/alert, not ignore repeated failures.

This backend is SYNCHRONOUS. Upstream may allocate String/buffers and read can wait
for individual bytes; its connect, subscribe and endMessage may block. Adapter chunk
size is a memory bound, not a hard time bound. A dependency timeout does not bound every
underlying board TLS operation. Whole-system RAM, malicious streaming timing, stack,
certificate storage and SDK cancellation require target-specific validation. Do not
use it as a hard realtime/nonblocking implementation or claim zero allocation globally.

For cooperative/nonblocking board operation, a different asynchronous backend/worker
and a proven bounded cancellation mechanism remain required. No board is cross-compiled
or physically tested by the native shim test; all board-family status stays NOT_RUN.

## Reproduce dependency contract tests

Clone the upstream repository separately and check out the exact SHA above. Run
`python3 tools/check_arduino_mqtt.py --upstream /path/to/ArduinoMqttClient --build-dir build-arduino-mqtt` for the pinned-source native test; add `--sanitize` in a separate build directory for sanitizer validation.
The shim substitutes only Arduino platform and byte transport. No Wi-Fi, TLS, physical
sensor or OS socket is present in this contract test. CP10 real TLS tests are separate
historical evidence and do not establish this Arduino backend's end-to-end behavior.

Primary source URLs:
- https://github.com/arduino-libraries/ArduinoMqttClient/blob/4699dda841c08b01036cd14412330bddc6c94edb/src/MqttClient.cpp
- https://github.com/arduino-libraries/ArduinoMqttClient/blob/4699dda841c08b01036cd14412330bddc6c94edb/src/MqttClient.h
- https://github.com/arduino-libraries/ArduinoMqttClient/blob/4699dda841c08b01036cd14412330bddc6c94edb/LICENSE.txt
