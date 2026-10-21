"""Shared golden fixtures and a nesting-depth-64 case against a Genesis checkout.

1. Fresh C++ envelopes equal tests/golden/messages.jsonl byte for byte.
2. Every golden envelope decodes with the host's codontrace.swarm.protocol.decode.
3. Fresh host-generated commands equal tests/golden/commands.jsonl (host drift check).
4. The C++ command probe decodes every golden command.
5. Depth 64: an envelope whose reading value nests 64 arrays deep is rejected by the
   host decoder (an exception, not a crash or acceptance). Without --strict-depth an
   acceptance is reported as KNOWN_GAP; the scheduled job runs with --strict-depth. The C++ side of the same
   case is the Unity test test_nesting_depth_64_is_capacity.
"""
import argparse, json, subprocess, sys, types
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('host', type=Path)
p.add_argument('build', type=Path)
p.add_argument('--strict-depth', action='store_true', help='fail if the host accepts depth 64')
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
golden = root / 'tests/golden'
for name, suffix in [('codontrace', ''), ('codontrace.swarm', 'swarm'), ('codontrace.genesis', 'genesis'), ('codontrace.contracts', 'contracts')]:
    if (a.host / 'src/codontrace' / suffix).is_dir():
        m = types.ModuleType(name); m.__path__ = [str((a.host / 'src/codontrace' / suffix).resolve())]
        sys.modules[name] = m
from codontrace.swarm.protocol import decode

fresh = (a.build / 'messages.jsonl').read_text()
want = (golden / 'messages.jsonl').read_text()
assert fresh == want, 'C++ envelopes differ from tests/golden/messages.jsonl'
lines = [l for l in want.splitlines() if l.strip()]
for l in lines:
    decode(l)
print(f'PASS: {len(lines)} golden C++ envelopes byte-identical and decoded by host')

host_cmds = json.loads((a.build / 'commands.json').read_text())['commands']
compact = [json.dumps(c, sort_keys=True, separators=(',', ':'), ensure_ascii=False) for c in host_cmds]
gcmds = [l for l in (golden / 'commands.jsonl').read_text().splitlines() if l.strip()]
assert compact == gcmds, 'host-generated commands differ from tests/golden/commands.jsonl'
r = subprocess.run([str(a.build / 'cts_command_probe')], input='\n'.join(gcmds) + '\n',
                   capture_output=True, text=True, timeout=10, check=True)
rows = [l for l in r.stdout.splitlines() if l.strip()]
assert len(rows) == len(gcmds), (len(rows), len(gcmds))
for cmd, row in zip(gcmds, map(json.loads, rows)):
    assert row['error'] == 'ok', row
    assert row['id'] == json.loads(cmd)['command_id'], row
print(f'PASS: {len(gcmds)} golden host commands unchanged and decoded by C++ probe')

reading = json.loads(lines[1])
deep = 0
for _ in range(63):
    deep = [deep]
reading['payload']['value'] = deep
try:
    decode(json.dumps(reading))
except Exception as e:  # rejection is the expected result
    print(f'PASS: depth-64 envelope rejected by host ({type(e).__name__})')
else:
    msg = ('host decode accepted an envelope nested 64 arrays deep '
           '(the C++ CommandDecoder rejects depth > 8 as Capacity)')
    if a.strict_depth:
        raise SystemExit('FAIL: ' + msg)
    print('KNOWN_GAP (not a pass): ' + msg)
