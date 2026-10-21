"""PlatformIO pre-script: link the native test binary with ASan/UBSan.

build_flags reach the compiler, but -fsanitize must also be on the link
line or the sanitizer runtimes are missing at link time.
"""
Import("env")  # type: ignore[name-defined]  # noqa: F821  (SCons global)

env.Append(  # type: ignore[name-defined]  # noqa: F821
    LINKFLAGS=["-fsanitize=address,undefined,float-cast-overflow", "-fno-sanitize-recover=all"]
)
