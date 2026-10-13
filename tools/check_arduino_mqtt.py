"""Test adapter against real pinned ArduinoMqttClient with synthetic Arduino/Client.

No network, dependency download, board compilation, TLS or physical sensor test.
Upstream LGPL source remains external and is not copied into the distribution.
"""
import argparse
import os
from pathlib import Path
import subprocess

PIN = "4699dda841c08b01036cd14412330bddc6c94edb"
p = argparse.ArgumentParser()
p.add_argument("--upstream", type=Path, required=True)
p.add_argument("--build-dir", type=Path, default=Path("build-arduino-mqtt"))
p.add_argument("--sanitize", action="store_true")
p.add_argument("--sanitizer-helper", type=Path,
               help="Optional environment-only helper, e.g. unavailable LeakSanitizer")
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
upstream = a.upstream.resolve()
build = a.build_dir.resolve()
build.mkdir(parents=True, exist_ok=True)
actual = subprocess.check_output(["git", "-C", str(upstream), "rev-parse", "HEAD"], text=True).strip()
if actual != PIN:
    raise SystemExit(f"ArduinoMqttClient commit mismatch: {actual} != {PIN}")
dirty = subprocess.check_output(["git", "-C", str(upstream), "status", "--porcelain", "--", "src"], text=True)
if dirty.strip():
    raise SystemExit("ArduinoMqttClient src must match clean pinned source")
flags = ["-std=gnu++11", "-Wall", "-Wextra", "-g"]
# Upstream uses GNU variable-length stack arrays, so do not claim ISO/pedantic compliance.
if a.sanitize:
    flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"]
elif a.sanitizer_helper:
    raise SystemExit("--sanitizer-helper requires --sanitize")
includes = [root/"tests/arduino_shim", upstream/"src", root/"src", root/"tests"]
sources = [root/"tests/arduino_mqtt_review.cpp", upstream/"src/MqttClient.cpp",
           root/"src/CodonTraceSwarm.cpp", *sorted((root/"src/cts").glob("*.cpp"))]
if a.sanitizer_helper:
    sources.append(a.sanitizer_helper.resolve())
exe = build/"arduino_mqtt_review"
command = [os.environ.get("CXX", "c++"), *flags,
           *["-I"+str(x) for x in includes], *map(str,sources), "-o", str(exe)]
subprocess.run(command, check=True)
subprocess.run([str(exe)], check=True, timeout=30)
print(f"PASS real upstream {PIN}; TEST shim/byte transport only; no board qualification", flush=True)
