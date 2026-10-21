"""Static half of the zero-heap claim: compile every library .cpp in src/ and fail
if any object references a heap allocation function (malloc family or operator new,
which std::string/std::vector storage would also pull in). operator delete is not an
allocation: a class with a virtual destructor gets a compiler-emitted deleting
destructor (D0) that references it even though the library never calls delete. The runtime half is tests/pio/test_no_heap."""
import re, subprocess, sys, tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[1]
cxx = sys.argv[1] if len(sys.argv) > 1 else 'c++'
bad = re.compile(r'^(malloc|calloc|realloc|posix_memalign|aligned_alloc|strdup|strndup|_Znw\w*|_Zna\w*)$')
sources = sorted(list((root/'src').glob('*.cpp')) + list((root/'src/cts').glob('*.cpp')))
found = 0
with tempfile.TemporaryDirectory() as t:
    for src in sources:
        obj = Path(t)/(src.stem + '.o')
        subprocess.run([cxx, '-std=c++11', '-O2', '-fno-exceptions', '-c', '-I'+str(root/'src'), str(src), '-o', str(obj)], check=True)
        syms = subprocess.run(['nm', '-u', str(obj)], capture_output=True, text=True, check=True).stdout.split()
        hits = [s for s in syms if bad.match(s)]
        if hits:
            found += 1
            print(f'FAIL {src.relative_to(root)}: {" ".join(sorted(set(hits)))}')
if found:
    raise SystemExit(f'{found} library object(s) reference heap functions')
print(f'PASS: {len(sources)} library objects reference no heap allocation function')
