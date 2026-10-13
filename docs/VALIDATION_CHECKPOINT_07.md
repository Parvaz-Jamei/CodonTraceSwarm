# Checkpoint 07 validation — 2026-10-13 UTC

| Gate | Result | Evidence |
|---|---|---|
| Native C++11 warnings-as-errors, eight suites | PASS: 6,383 assertions | evidence/native_and_host.log |
| Parser synthetic fuzz | PASS: 20,000 inputs, separately counted | same log |
| Actual pinned-host integrations | PASS | same log |
| Real TLS MQTT broker with two C++ nodes | PASS: 11 scenarios | evidence/broker_lab/ |
| ASan/UBSan on the live C++ node path | PASS, UBSan recovery disabled | broker results and C++ diagnostics |
| Independent checkpoint07 review | PENDING: tester quota exhausted | not release-approved |
| CMake frontend / hosted CI / MCU builds | NOT_RUN | configurations only |
| Physical sensors / Wi-Fi / ESP32 deployment | NOT_IMPLEMENTED / NOT_RUN | roadmap |

Host: 74c7b3f15c2d093da03e392094814d63a4997c2c. Broker aMQTT 0.12.1,
Paho 2.1.0, MQTT 3.1.1, QoS1, negotiated TLS1.3 on loopback.
The host checkout is external, pinned, and not included in this library package.

The 6,383 native checks are assertions, not 6,383 independent scientific studies.
Broker scenarios are integration checks with synthetic sensor data (42.5). No
hardware qualification, sensor accuracy or scientific discovery is claimed.

The final run initially encountered a local executable-permission error before
starting C++ nodes. Restoring the built binary's executable bit and rerunning the
unchanged final sources passed all eleven scenarios; this was not a library fix.

ASan/UBSan covered the new live node path in this checkpoint. Eight-suite sanitizer
results from checkpoint06 remain historical in evidence/checkpoint06 and
 docs/VALIDATION_CHECKPOINT_06.md; they are not relabeled as new runs. LSan was
not run: the sandbox helper disables it due to unavailable /proc. Symbolization
warnings remain visible. The helper is environment-only, not product code.

The controlled restart test is not sudden-link-loss/in-flight recovery. No production
ESP32 network or time adapter is provided. The Python subprocess bridge is test-only.
See docs/BROKER_LAB.md for reproducing the run and the complete boundaries.
