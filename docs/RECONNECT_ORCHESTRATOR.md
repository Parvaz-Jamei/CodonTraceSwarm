# Reconnect orchestrator — checkpoint 12

`cts::ReconnectOrchestrator` is the single-threaded owner that ties `ReconnectPolicy`, `NonblockingAttempt`, `NetworkInbox`, `Device`, and `CommandSession` to an injected `QosPublisher`. It does not open sockets. It allocates nothing and starts no threads.

## Rules

- `clockReady == false` never starts a connect attempt. The refusal counter increments. This is independent of hoping the policy stays quiet.
- A new connection generation serializes a **fresh** `Device` hello before any reading. Readings return `HelloRequired` until that hello's full write and QoS1 step have finished. The device sequence is not reset.
- If a `CommandSession` reply is already pending, those exact bytes are published before the new hello, because the codec holds serialization until `confirmReplyPublished`. The reply is not discarded to make room for hello.
- `confirmReplyPublished` runs only after the publisher reports a full write **and** QoS1 completion for that reply's sequence. A failed step, a write without QoS1, or a short write leaves the reply pending.
- Disconnect or cancel while a publish is in flight increments `losses()`. That publish is not a delivered message and must not be confirmed.
- An old generation token passed to `onDisconnected` is stale. It does not close the current attempt or add a loss.
- If the handshake reaches `Succeeded` but the clock is not ready or `NetworkInbox::connected` fails, the orchestrator abandons that generation before `policy.lost`. Abandon cancels the client for both `Running` and `Succeeded`, then marks the attempt dead. The next due poll does not call `start` while that client is still live.
- Once `Device::reading` has serialized, those exact bytes are retried if the publisher step fails. `publishReading` returns `Busy`, not `Failed`, and `readingsAllowed()` is false while that retry is queued. A caller who sees `Failed` can rely on those bytes not being published later.

`QosPublisher::step` must not confirm replies itself. `ArduinoMqttAdapter::publishReply` is a different, blocking path and is not called by this orchestrator.
