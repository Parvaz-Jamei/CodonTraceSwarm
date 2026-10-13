# Nonblocking connect attempt — checkpoint 12

`cts::NonblockingAttempt` is a single-threaded state machine around an injected `ConnectAttemptClient`. It has no sockets, sleeps, threads, `String`, or heap traffic of its own.

`start` begins one generation. `poll` reports partial progress while the client is not done. `cancel` aborts the running generation and still ignores every other phase, including `Succeeded`. A poll at or after the timeout cancels and returns `TimedOut` without treating the client as connected. Time rollback, a client `Fail`, and a finished handshake that is not ok also cancel the client before the phase leaves `Running`. `poll` and `cancel` for any other generation increment `staleIgnored` and leave the running attempt unchanged.

`start` returns `Fault` and does not open a client when `now > UINT64_MAX - timeoutMs`, because that deadline cannot be represented. If a running attempt already has an unrepresentable deadline, `poll` cancels the client and returns `TimedOut` instead of staying in progress. `abandon` cancels a `Running` or `Succeeded` generation and marks that attempt dead so a later `start` cannot stack a second live client.

## Not the Arduino MQTT adapter

`cts::ArduinoMqttAdapter` does **not** implement `ConnectAttemptClient` or `NonblockingAttempt`. Its `receive`, `publishReply`, and the upstream `poll` / `beginMessage` / `write` / `endMessage` calls are synchronous and may block, including a PUBACK wait inside `endMessage`. Do not describe that adapter as this seam.
