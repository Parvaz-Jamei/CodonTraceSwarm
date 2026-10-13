# CP06 independent review

Scope: portable NetworkInbox and real CommandSession linkage. No physical radio, live broker, TLS, or concurrent task execution claimed.

## Defect found and fixed

CP06-01: inbox route was independently configurable from its target CommandSession. An inbox bound to other/other could admit a lab/node command body into the lab/node session despite the mismatch between authenticated topic and target route. Reported to implementer; fixed with CommandSession::matchesRoute and pollOne ScopeMismatch check. Independent regression confirms no admission, no execution, and retained queue on mismatch.

## Executed validation

`tests/network_review.cpp`: 87 independent checks PASS with strict GCC C++11 warnings-as-errors and separately ASan/UBSan. LeakSanitizer NOT RUN because sandbox lacks /proc; helper outside product disables LSan. Sanitizer log: sanitizer.log.

Cases: wrong topic, unknown retain status, untrusted/retained input, stale connection generation, reconnect discards, fragmented message with absent continuation topic, byte ownership after caller mutation, gaps, size overflow and invalid token, queue full, clock-unavailable retry preserving queue, no executor calls inside push or pollOne, blocked reply ordering, and wrong session route.

No remaining reproducible blocker found within inspected paths. This does not qualify TLS, broker ACL, device Wi-Fi, adapter retain metadata, multi-task safety, or physical board memory/latency.
