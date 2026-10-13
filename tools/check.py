"""Portable native verification entrypoint; requires Python3 and GCC/Clang."""
import argparse
from pathlib import Path
import subprocess
import os
import sys

p = argparse.ArgumentParser()
p.add_argument('--host', type=Path, help='Pinned original Genesis checkout')
p.add_argument('--sanitize', action='store_true')
p.add_argument('--build-dir', type=Path, default=Path('build-native'))
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
build = a.build_dir.resolve()
build.mkdir(parents=True, exist_ok=True)
compiler = os.environ.get('CXX', 'c++')
flags = ['-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-g']
if a.sanitize:
    flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
exe = build / 'cts_tests'
core = [root/'src/CodonTraceSwarm.cpp', root/'src/cts/Json.cpp', root/'src/cts/Command.cpp', root/'src/cts/Dispatcher.cpp', root/'src/cts/CommandSession.cpp']
sources = [*core, root/'tests/native.cpp']
subprocess.run([compiler, *flags, '-I'+str(root/'src'), *map(str,sources), '-o', str(exe)], check=True)
messages = build / 'messages.jsonl'
subprocess.run([str(exe), str(messages)], check=True)
for target, source in [('cts_session_tests', 'session.cpp'), ('cts_session_probe', 'session_probe.cpp'), ('cts_dispatcher_tests', 'dispatcher.cpp'), ('cts_command_tests', 'commands.cpp'), ('cts_command_probe', 'command_probe.cpp')]:
    subprocess.run([compiler, *flags, '-I'+str(root/'src'), *map(str,core), str(root/'tests'/source), '-o', str(build/target)], check=True)
subprocess.run([str(build/'cts_command_tests')], check=True)
subprocess.run([str(build/'cts_dispatcher_tests')], check=True)
subprocess.run([str(build/'cts_session_tests')], check=True)
if a.host:
    subprocess.run([sys.executable, str(root/'tools/verify_host.py'), str(a.host.resolve()), str(messages)], check=True)
    subprocess.run([sys.executable, str(root/'tools/verify_commands.py'), str(a.host.resolve()), str(messages), str(build/'cts_command_probe'), '--evidence', str(build/'commands.json')], check=True)
    subprocess.run([sys.executable, str(root/'tools/verify_session.py'), str(a.host.resolve()), str(build/'cts_session_probe'), '--evidence', str(build/'roundtrip.json')], check=True)
else:
    print('NOT_RUN: original-host verification. Supply --host /path/to/pinned/genesis.')
