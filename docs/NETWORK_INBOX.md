# Bounded transport-to-command handoff — checkpoint 06

`NetworkInbox` connects received bytes to the existing CommandSession without executing
JSON parsing, publishing or driver callbacks inside push(). It is portable C++ logic,
NOT an MQTT client, Wi-Fi driver, broker ACL or TLS implementation.

## Binding and connection lifetime

Bind once to swarm/node. The only accepted topic is exactly
`genesis/swarm/<swarm>/<node>/down`, matching pinned Genesis MQTTLink.publish_command.
No wildcard, prefix or payload-based rerouting is allowed. pollOne also verifies that
the target CommandSession belongs to the SAME live swarm/node; the decoder separately
checks boot_id. A mismatched/unstarted session returns ScopeMismatch without consuming
queued bytes. Fix the binding/session or explicitly discard via disconnected().

After successful authenticated transport setup, call connected(generation) with a
strictly increasing nonzero LOCAL connection generation. This is not MQTT's reusable
packet ID. Duplicate/old generation calls fail without clearing active state. A new
generation clears old complete and partial receive data with loss counters. Call
 disconnected() on loss; packets stamped with an old generation cannot enter afterward.
This clears only NetworkInbox: commands already admitted to Dispatcher retain its TTL
and deduplication rules. It does not cancel hardware actions or clear reply outboxes.

## Adapter metadata contract

Each ReceiveFragment includes explicit byte lengths, full message size, offset, an
adapter-assigned per-publish token, connection generation and trusted delivery flags.
The default flags reject. authenticated means the ADAPTER verified the connection,
identity/ACL and source route. It is not derived from JSON and is not cryptography
performed here. retainKnown=false is rejected; do not mark it true for a backend that
hides the retain flag. Broker policy alone is not proof a hidden flag was observed.

The token remains stable across one publish's fragments. It is not used for command
deduplication; that remains scoped command_id plus decoded semantic content. An adapter
must define tokens even for QoS0, whose packet identifier may be zero.

The first fragment must include exact topic bytes. Continuations can omit topic, as
ESP-MQTT does, or repeat the same exact topic. All fragments must supply consistent
adapter metadata. No NUL termination is needed; embedded payload NUL reaches the strict
parser and is rejected there. Only complete copied messages are visible to pollOne.

## Capacity and loss policy

- Three complete frames, each at most 4,096 bytes; one additional 4,096-byte assembly.
- One in-flight assembly; offsets must be contiguous with matching token and total size.
- Gaps, overlaps, zero-length fragments, bad pointers/lengths and oversized totals reject.
- A new offset-zero fragment abandons an unfinished assembly, counted explicitly.
- Full inbox rejects new starts without evicting complete frames. There is no silent
  unbounded backlog or heap allocation in NetworkInbox.
- push copies bytes before returning; caller buffers may be overwritten afterward.
- pollOne submits at most one completed frame and does not execute its driver. Call
  session.poll separately in the main loop to perform authorized work and generate replies.
- Temporary clock unavailability, dispatcher capacity or session Busy retains the head
  frame and reports Retrying. Permanent parse/scope/policy errors drain it as Rejected.
  Inspect nested SessionResult/DispatchResult for cause; no wire rejection reply is invented.
- FIFO head-of-line blocking is deliberate and bounded. Disconnect explicitly drops
  receive backlog. Expiry is checked at eventual admission, so backlog is not retimestamped.

Counters fragments/enqueued/drained/retries/rejected/abandoned/disconnectDrops/full
measure different events and must not be summed as a total count of unique commands.
`rejected` counts push-stage rejection, while drained may include parser rejection.
A partial frame lost on disconnect increments both abandoned and disconnectDrops.

## Task and callback safety

This class is single-owner, not thread-safe. Its reentry guard prevents mutation from
callbacks invoked by pollOne; this does not synchronize CPU tasks. An actual ESP-MQTT
adapter must use a serialized event handoff or an external lock around ALL inbox access.
Do not concurrently call push from the MQTT task and pollOne from the Arduino task.
No real SDK task adapter is shipped or qualified in this checkpoint.

Network transport still must preserve Device sequence and the pending-reply contract.
Do not reconstruct replies on every retry or announce publication before ordered delivery.
No broker acknowledgment, authentication, TLS certificate verification, reconnect timer
or sensor acquisition is performed by the inbox itself.

## Tests and source basis

Main tests: every two-part split of a real command, byte ownership, 4,096-byte boundary,
queue full, clock backpressure, partial discard and malformed input. Independent tester
found topic/session binding mismatch in the first implementation; matchesRoute and a
regression now close it. Independent tests cover 87 adversarial assertions.

The real-host roundtrip runs through `network_probe.cpp`, splitting host commands into
seven-byte fragments, then actual CommandSession callbacks/readings/replies. It still
uses pipes and a synthetic sensor; no real MQTT events or sockets are simulated as proof.

Native sizeof(NetworkInbox)=16,824 bytes. Combined with Device/CommandSession and clock
gates the native object footprint is about 64 KB before TLS, drivers and task stacks.
This is not an MCU memory map; do not place these objects on a small callback stack.

Primary documentation consulted 2026-10-13 Asia/Tehran:
https://docs.espressif.com/projects/esp-mqtt/en/latest/esp32/
Specifically MQTT_EVENT_DATA: total_data_len/current_data_offset, retain, and topic
present only in the first fragment. Code uses our own portable metadata type and does
not claim a pinned SDK adapter. Actual host contract remains commit
74c7b3f15c2d093da03e392094814d63a4997c2c, src/codontrace/swarm/mqtt.py.
