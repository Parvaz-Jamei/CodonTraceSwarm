"""Depth-64 rejection check for a host decoder.

Builds a reading envelope whose value nests 64 arrays deep and passes it to the
decoder. The host must raise ValueError. Acceptance, or any other exception
(TypeError, RecursionError, ...), is a failure: those are bugs, not rejections.
The C++ side of the same case is the Unity test test_nesting_depth_64_is_capacity.

Usage: python3 tools/depth_check.py <genesis checkout>   (exit 0 = rejected)
"""
import json
import sys
import types
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def deep_envelope(reading_line, depth=64):
    reading = json.loads(reading_line)
    value = 0
    for _ in range(depth - 1):
        value = [value]
    reading['payload']['value'] = value
    return json.dumps(reading)


def depth64_rejected(decode, reading_line):
    """Return (ok, detail). ok is True only if decode raised ValueError."""
    try:
        decode(deep_envelope(reading_line))
    except ValueError as e:
        return True, f'ValueError: {e}'
    except Exception as e:  # noqa: BLE001 - reported as a failure, not a rejection
        return False, f'raised {type(e).__name__} instead of ValueError: {e}'
    return False, 'accepted an envelope nested 64 arrays deep (C++ rejects depth > 8 as Capacity)'


def load_decode(host):
    for name, suffix in [('codontrace', ''), ('codontrace.swarm', 'swarm'), ('codontrace.genesis', 'genesis'), ('codontrace.contracts', 'contracts')]:
        if (host / 'src/codontrace' / suffix).is_dir():
            m = types.ModuleType(name)
            m.__path__ = [str((host / 'src/codontrace' / suffix).resolve())]
            sys.modules[name] = m
    from codontrace.swarm.protocol import decode
    return decode


def golden_reading():
    lines = [l for l in (ROOT / 'tests/golden/messages.jsonl').read_text().splitlines() if l.strip()]
    return lines[1]


if __name__ == '__main__':
    ok, detail = depth64_rejected(load_decode(Path(sys.argv[1])), golden_reading())
    print(('PASS: depth-64 envelope rejected by host (' if ok else 'FAIL: host ') + detail + (')' if ok else ''))
    sys.exit(0 if ok else 1)
