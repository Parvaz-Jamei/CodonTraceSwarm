"""Static half of the zero-heap claim: compile every library .cpp in src/ and fail
if any object references a heap allocation function (malloc family or operator new,
which std::string/std::vector storage would also pull in). It also compiles
tests/heap_symbols/*.cpp, which emit every header-only adapter (all sensors/*.h and
an explicit ArduinoMqttAdapter instantiation) against declaration-only vendor fakes
in tests/fakes, so only the adapter code itself is inspected. operator delete is not an
allocation: a class with a virtual destructor gets a compiler-emitted deleting
destructor (D0) that references it even though the library never calls delete. The runtime half is tests/pio/test_no_heap."""
import re, subprocess, sys, tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[1]
cxx = sys.argv[1] if len(sys.argv) > 1 else 'c++'
bad = re.compile(r'^(malloc|calloc|realloc|posix_memalign|aligned_alloc|strdup|strndup|_Znw\w*|_Zna\w*)$')
sources = sorted(list((root/'src').glob('*.cpp')) + list((root/'src/cts').glob('*.cpp')))
headers = sorted((root/'tests/heap_symbols').glob('*.cpp'))
expected = {'fdcx', 'mqtt_adapter', 'sensors'}
assert {h.stem for h in headers} == expected, [h.stem for h in headers]
found = 0
with tempfile.TemporaryDirectory() as t:
    for src in sources + headers:
        obj = Path(t)/(src.stem + '.o')
        subprocess.run([cxx, '-std=c++11', '-O2', '-fno-exceptions', '-c', '-I'+str(root/'src'), '-I'+str(root/'tests/fakes'), str(src), '-o', str(obj)], check=True)
        if src in headers:
            defined = subprocess.run(['nm', '--defined-only', str(obj)], capture_output=True, text=True, check=True).stdout
            assert any(f' {k} ' in defined for k in 'TW'), f'{src.name} emitted no code'
        syms = subprocess.run(['nm', '-u', str(obj)], capture_output=True, text=True, check=True).stdout.split()
        hits = [s for s in syms if bad.match(s)]
        if hits:
            found += 1
            print(f'FAIL {src.relative_to(root)}: {" ".join(sorted(set(hits)))}')
if found:
    raise SystemExit(f'{found} library object(s) reference heap functions')
print(f'PASS: {len(sources)} library objects and {len(headers)} header-adapter objects reference no heap allocation function')
