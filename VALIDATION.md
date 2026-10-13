# Checkpoint 11 validation — 2026-10-13

| Gate | Actual result | Evidence |
|---|---|---|
| Existing6,383 native assertions +20,000 fuzz inputs | PASS | evidence/native_and_host.log |
| Millis32, reconnect/inbox and independent290 | PASS | same log |
| Actual pinned host integrations | PASS | same log |
| Adapter against actual ArduinoMqttClient0.1.8 source | PASS:171 independent assertions | evidence/cp11_independent/native.log |
| Same adapter suite ASan/UBSan | PASS:171; recovery disabled | evidence/cp11_independent/sanitized.log |
| Independent scope/source review | Completed; material concurrency issue corrected | evidence/cp11_independent/REVIEW.md |
| Board compile / physical Wi-Fi / TLS with this adapter | NOT_RUN | native shims only |

Material correction: polling-only receive would lose inbound PUBLISH while upstream
endMessage waits for PUBACK. Callback-owned queue-only receive preserves it. Regression
uses actual upstream parser and synthetic MQTT byte stream; executor remains outside
callback. Tests additionally exercise partial headers, delayed payload, bad routing,
retained/empty/oversized input, short writes, missing/wrong PUBACK and callback ownership.

External ArduinoMqttClient source SHA4699dda841c08b01036cd14412330bddc6c94edb is not
bundled. Test shims replace Arduino platform/transport, not upstream MQTT code.
No real network/broker/MCU/TLS is involved in this new contract test. CP10 real broker
results remain historical and do not validate this Arduino backend end-to-end.

LSan disabled with external environment helper due to unavailable /proc; symbolization
warnings retained. Upstream uses GNU stack VLAs; runner uses gnu++11, not pedantic ISO
qualification. Optional synchronous adapter may allocate through upstream and block
per byte; no global time bound or zero-allocation claim.

Ordinary core binaries were built with normal check.py. Local execute-bit failures
interrupted initial runs; execute bits on owned ELF outputs were restored and all
compiled suites and five host verification commands rerun successfully, without
changing code or relaxing assertions. Final log is the successful complete execution.

Host test pin74c7b3f15c2d093da03e392094814d63a4997c2c; fetched mainfa7e7e995afe3ad7c0970ed783948d0336ac91f3.
One unrelated pre-existing untracked run temporary file remains untouched. No host
source patch or push. Phase02 remains PARTIAL; see RESUME_FA.md for remaining gates.
