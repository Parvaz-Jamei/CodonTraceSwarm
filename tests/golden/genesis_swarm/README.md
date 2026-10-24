# Vendored Genesis swarm wire fixtures

Copied byte for byte from `codontrace-genesis` `tests/golden/swarm/` at the commit in
`GENESIS_REV` (branch `fix/swarm-value-strict`, not yet on Genesis main), together
with its `MANIFEST.sha256`. Do not edit these files here; bump `GENESIS_REV`, re-copy
and commit the new manifest in one change.

`tools/verify_genesis_fixtures.py <genesis checkout at GENESIS_REV> <build dir>`
(run by the "loopback contract" job in `.github/workflows/native.yml`) checks:

1. The checkout is at `GENESIS_REV` and its `MANIFEST.sha256` equals the vendored one byte for byte.
2. `sha256sum -c MANIFEST.sha256` passes for the vendored copy and for the checkout,
   and the manifest lists every fixture file (no unlisted extras).
3. The seven `valid/v2_*` frames Genesis attributes to the C++ encoder are lines of
   the freshly built encoder output (`messages.jsonl`). `v2_waveform_256.json` and
   `v1_scalar.json` are host-written frames and are not checked against the encoder.
4. With that checkout's `codontrace.swarm.protocol.decode`, every `valid/` frame
   decodes and every `invalid/` frame raises `ValueError`.
