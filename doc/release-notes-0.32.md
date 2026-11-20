# Luar 0.32 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends nested field folding to four scalar levels.

## Highlights

- **Four-level reads**: `a.b.c.d.x` is resolved at record time and executed
  as one `CGETFIELD` with the combined offset.
- The recorder skips all three intermediate `GETFIELD` opcodes.
- `tests/ffi/22_trace_cdata.lua` covers the four-level read.
- Deeper paths and four-level writes still fall back.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make test-asan
make shared test-shared
tests/upstream/run.sh
make dist && make test-dist
```

GitHub Actions also runs the macOS shared/install/module job.

## Known limitations

- Four-level writes and deeper paths fall back.
- Pointer arithmetic beyond indexing falls back.
- `B == 0` calls and continuation error/yield paths remain unsupported.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
