# Research log

Checked 2026-10-13 Asia/Tehran (UTC 2026-10-12).

| Question | Primary source | Design consequence |
|---|---|---|
| What does the actual host accept? | https://github.com/Parvaz-Jamei/codontrace-genesis/tree/74c7b3f15c2d093da03e392094814d63a4997c2c/src/codontrace/swarm | Pin source and test actual generated C++ messages through its decoder/coordinator. Do not copy engine code into MIT library. |
| Can malformed or duplicate JSON keys be accepted safely? | https://www.rfc-editor.org/rfc/rfc8259.html | Validate UTF-8 and escape JSON text. Future command parser must reject duplicate keys; no command handling is claimed before that gate. |
| Does generic Arduino JSON handling guarantee bounded heap? | https://arduinojson.org/v7/api/jsondocument/ | Default allocation exists; select bounded independent serialization for this checkpoint. No blanket promise about future TLS/MQTT heap. |

This is interoperability and correctness engineering. Neither source inspection nor an unsuccessful novelty search proves a new scientific discovery. Interdisciplinary research (measurement uncertainty, distributed delivery semantics, control and reproducible experiments) belongs in separately specified tests after this foundation works.

Checkpoint 02: fetched origin/main again; still 74c7b3f. Inspected original runtime
_command_params and 60-second lifetime rule, plus uploaded SESSION_COMMANDS contract.
Re-read RFC8259 string comparison and interoperability requirements. Adopted strict
reject-on-duplicate semantics after Unicode escape decoding; command identity does not
use lossy float timestamps or borrowed pointer storage. NUL is rejected as a deliberate
C-string profile restriction even when represented by a syntactically valid escape.

## 2026-10-13 Asia/Tehran — checkpoint 03

Fetched origin/main; HEAD and remote still 74c7b3f15c2d093da03e392094814d63a4997c2c;
working tree clean. Read src/codontrace/swarm/runtime.py command generation/expiry and
current library parser/gates. No host source copied into the MIT implementation.

Question: How should retries and delayed execution be handled on bounded-memory boards?
Primary sources opened: OASIS MQTT 5.0 official standard, QoS and expiry sections
https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html
and Espressif ESP Timer official ESP32 guide
https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/esp_timer.html
Design inference: transport delivery does not replace application identity checks;
monotonic elapsed time should independently bound queued command life. Deep sleep
requires new boot/session handling. Implementation uses conservative dual deadlines
and bounded history; it does not claim a novel algorithm or durable exactly-once execution.

## 2026-10-13 Asia/Tehran — checkpoint 04

Fetched remote; HEAD=origin/main=74c7b3f15c2d093da03e392094814d63a4997c2c, clean tree.
Read actual swarm/protocol.py reply keys/types and runtime.py expiration-before-ingest,
identity match and pending-only reply acceptance. Used existing contract, copied no host
implementation into the MIT library. Host fixture remains an external pinned checkout.

Primary-source lookup: OASIS MQTT 5.0 request/response section 4.10
https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html
Question: Is transport acknowledgment sufficient to assert application command success?
Design inference: no; retain separate admission, callback outcome and host acceptance.
Genesis command_id correlates application results; this stage does not implement MQTT
Response Topic/Correlation Data properties or TLS. Cross-domain reliability ideas are
applied as engineering practices, not claimed as new scientific findings.

## 2026-10-13 Asia/Tehran — checkpoint05

User reattached authoritative checkpoint04 and target plan V2; zip hashes matched local
source. Read AGENTS.md, Phase01/02, resource/API and session contracts. Remote fetch still
matches 74c7b3f15c2d093da03e392094814d63a4997c2c; no host mutation.
Primary-source lookup: Espressif System Time, instant/smooth SNTP corrections and sync
callback semantics: https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32/api-reference/system/system_time.html
Existing ESP Timer reference: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/esp_timer.html
Inference: sync status alone must not silently stand in for acquisition-time quality;
preserve original timestamp, explicit uncertainty/age and discontinuity handling.
Implementation is an engineering validator, not a new clock-estimation algorithm.

## 2026-10-13 Asia/Tehran — checkpoint06

Checkpoint05 and complete target plan were present. Fetched host main still equals
74c7b3f15c2d093da03e392094814d63a4997c2c; working tree clean. Inspected actual
swarm/mqtt.py topic routing and command publishing contract.
Primary-source lookup: https://docs.espressif.com/projects/esp-mqtt/en/latest/esp32/
MQTT_EVENT_DATA fragmentation exposes total_data_len/current_data_offset and retain;
only first fragment includes topic. Designed portable bounded handoff around these
requirements, without claiming an SDK binding or treating JSON flags as authentication.
No claim that MQTT or bounded queues are a novel scientific algorithm.
