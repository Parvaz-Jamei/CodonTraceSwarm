"""Check actual C++ fixture messages using untouched pinned Genesis modules.

Namespace packages only bypass unrelated top-level optional imports. Protocol,
normalization, coordinator and journal implementations are real host code.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import types

import os
# CTS_HOST_REV: Genesis revision under test (default: the pinned contract host).
PIN = os.environ.get('CTS_HOST_REV', '3e6686d72834221f3720fe54314aa84d2ea0ec05')
p = argparse.ArgumentParser()
p.add_argument('host', type=Path)
p.add_argument('messages', type=Path)
a = p.parse_args()
head = subprocess.check_output(['git', '-C', str(a.host), 'rev-parse', 'HEAD'], text=True).strip()
if head != PIN:
    raise SystemExit(f'Wrong host revision: {head}; expected {PIN}')
for name, suffix in [('codontrace', ''), ('codontrace.swarm', 'swarm'), ('codontrace.genesis', 'genesis')]:
    module = types.ModuleType(name)
    module.__path__ = [str((a.host / 'src/codontrace' / suffix).resolve())]
    sys.modules[name] = module
from codontrace.swarm.protocol import decode
from codontrace.swarm.runtime import SwarmCoordinator
from codontrace.swarm.sensors import normalize

rows = [decode(line) for line in a.messages.read_text().splitlines() if line.strip()]
assert len(rows) == 7, len(rows)
assert rows[0]['kind'] == 'hello'
assert [row['seq'] for row in rows] == list(range(7))
channels = rows[0]['payload']['channels']
assert rows[0]['payload']['operations'] == [], 'No unimplemented commands may be advertised'
assert {c['descriptor']['value_type'] for c in channels.values()} == {'scalar','vector','waveform','boolean','category'}
readings = [normalize(row['payload'], channels[row['payload']['channel']]) for row in rows[1:]]
assert readings[0]['value'] == 0 and readings[0]['valid'] is True
assert readings[1]['value'] is None and readings[1]['valid'] is False
assert readings[2]['value'] == [-1, 2.25]
assert readings[3]['value'] == [-1, 2.25, 0]
assert readings[4]['value'] is False
assert isinstance(readings[5]['value'], str) and any(ord(c) > 127 for c in readings[5]['value'])
now = rows[1]['payload']['sample_ms']
swarm = rows[0]['swarm_id']
first = SwarmCoordinator(swarm)
second = SwarmCoordinator(swarm)
for row in rows:
    assert first.ingest(row, now_ms=now)['accepted']
    assert second.ingest(row, now_ms=now)['accepted']
assert first.snapshot(now_ms=now) == second.snapshot(now_ms=now)
assert first.ingest(rows[-1], now_ms=now)['accepted'] is False
print(f'PASS: {len(rows)} actual C++ envelopes, all five types, zero/null, Unicode, real coordinator acceptance and deterministic same-input replay. Host={PIN}')
print('SYNTHETIC codec fixtures; no real hardware, MQTT or ALife trajectory is tested.')
