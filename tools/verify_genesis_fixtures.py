"""Verify the vendored Genesis swarm fixtures (tests/golden/genesis_swarm) against a
Genesis checkout pinned to tests/golden/genesis_swarm/GENESIS_REV. See that README."""
import argparse, hashlib, subprocess, sys, types
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('genesis', type=Path)
p.add_argument('build', type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
vend = root / 'tests/golden/genesis_swarm'
rev = (vend / 'GENESIS_REV').read_text().strip()
ENCODER = ['v2_hello_five_types', 'v2_scalar_zero', 'v2_vector', 'v2_waveform',
           'v2_boolean', 'v2_invalid_null', 'v2_category_unicode']
fail = []

head = subprocess.run(['git', '-C', str(a.genesis), 'rev-parse', 'HEAD'], capture_output=True, text=True, check=True).stdout.strip()
if head != rev:
    sys.exit(f'genesis checkout is {head}, pinned GENESIS_REV is {rev}')
up = a.genesis / 'tests/golden/swarm'
manifest = (vend / 'MANIFEST.sha256').read_bytes()
if (up / 'MANIFEST.sha256').read_bytes() != manifest:
    fail.append('vendored MANIFEST.sha256 differs from Genesis at ' + rev)

def check(base):
    listed = set()
    for line in manifest.decode().splitlines():
        digest, name = line.split(None, 1)
        name = name.lstrip('*'); listed.add(name)
        got = hashlib.sha256((base / name).read_bytes()).hexdigest()
        if got != digest:
            fail.append(f'{base}/{name}: sha256 {got} != manifest {digest}')
    present = {str(f.relative_to(base)) for d in ('valid', 'invalid') for f in (base / d).rglob('*') if f.is_file()}
    if present != listed:
        fail.append(f'{base}: files not matching manifest: {sorted(present ^ listed)}')
    return listed
listed = check(vend)
check(up)

fresh = set((a.build / 'messages.jsonl').read_text().splitlines())
for n in ENCODER:
    frame = (vend / 'valid' / (n + '.json')).read_text().rstrip('\n')
    if frame not in fresh:
        fail.append(f'valid/{n}.json is not a line of the fresh C++ encoder output')

for name, suffix in [('codontrace', ''), ('codontrace.swarm', 'swarm'), ('codontrace.genesis', 'genesis'), ('codontrace.contracts', 'contracts')]:
    if (a.genesis / 'src/codontrace' / suffix).is_dir():
        m = types.ModuleType(name); m.__path__ = [str((a.genesis / 'src/codontrace' / suffix).resolve())]
        sys.modules[name] = m
from codontrace.swarm.protocol import decode
for name in sorted(listed):
    data = (vend / name).read_text()
    if name.startswith('valid/'):
        try:
            decode(data)
        except Exception as e:  # noqa: BLE001
            fail.append(f'{name}: expected to decode, raised {type(e).__name__}: {e}')
    else:
        try:
            decode(data)
            fail.append(f'{name}: expected ValueError, decoded')
        except ValueError:
            pass
        except Exception as e:  # noqa: BLE001
            fail.append(f'{name}: expected ValueError, raised {type(e).__name__}')

for f in fail:
    print('FAIL', f)
if fail:
    sys.exit(1)
print(f'genesis fixtures OK at {rev}: {len(listed)} files sha256-pinned, {len(ENCODER)} encoder frames, all decode outcomes as labelled')
