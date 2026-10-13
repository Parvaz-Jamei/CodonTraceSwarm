# Independent checkpoint 10 review

Scope: the desktop `@policy` bridge in `tests/broker_node.cpp` and the two
appended real-broker recovery scenarios in `tools/broker_lab/run.py`.
The production library was not modified by this reviewer.

## Result

**All 13 live broker scenarios passed in an independently launched run.**
The two additions exercise abrupt broker process termination, an actual
disconnect callback, two real connection-refused attempts while the broker is
offline, and a subsequent TLS connection followed by an actual C++ hello,
Genesis ingestion, a new command, its response, and duplicate suppression.
No material defect identified within this test-harness scope.

The trace records four attempts and three failures. An additional inspection
of the recorded dispatch timestamps found every retry at or after its current
policy deadline. Caller-selected zero jitter produces 500, 1000, and 2000 ms
scheduled waits in this run; these are not sampled randomness or a statistical
test. The observed TLS version was TLSv1.3. Final host command states were
three acknowledged commands and one deliberately rejected callback failure.

The current source also removes earlier `results.json` and
`reconnect_trace.json` before validation. This prevents an aborted rerun from
leaving those previous PASS artifacts behind. The recorded hashes identify
reviewed sources and the supplied executable; the executable itself is not
included in evidence.

## Important scope limits

- The initial Connect action adopts an already-established lab connection.
  Subsequent offline and recovery attempts are gated by the C++ policy.
- Paho auto-reconnect is disabled. Network work is still synchronous desktop
  Paho, with finite connection and event waits, not a nonblocking MCU adapter.
- The lab sets clockReady true, supplies jitter zero, and uses a synthetic
  sensor with a constant reading. It does not qualify NTP, board clocks,
  entropy, hardware sensors, or field performance.
- The abrupt kill occurs between commands. It is not an in-flight PUBACK loss,
  durable command journal, or persistent MQTT session recovery test.
- Only Node A participates in the new policy reconnect path; Node B was
  exercised in the earlier two-node portion of the same run.
- Transport generation is updated by the bridge on accepted online events;
  actual OS callbacks are relayed by the Python harness. This does not provide
  a deployed board-specific network implementation.
- Earlier CP09 independent unit tests cover stale tokens, clock rollback,
  timeout thresholds, jitter bounds and overflow. Those are not independently
  induced over the network in these two new scenarios.

## Reproduction

```sh
PYTHONPATH=rebuild_work/broker_v121 python3 \
  CodonTraceSwarm/tools/broker_lab/run.py \
  --host rebuild_work/host \
  --node rebuild_work/cp10/cts_broker_node \
  --output rebuild_work/cp10_independent_live
```

The output directory and dependency path are reviewer workspace paths;
use an installed test environment and locally built executable elsewhere.
Evidence contains the full scenario log, final report and reconnect trace.
Expected negative TLS/authentication diagnostics are preserved in broker.log.
