# Luar 0.33 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes four-level scalar field access.

## Highlights

- **Four-level writes**: `a.b.c.d.x = i` and `a.b.c.d.y = 2.5` lower to one
  `CSETFIELD`/`CSETFIELDK` with the combined offset.
- The recorder skips all three intermediate `GETFIELD` opcodes.
- `tests/ffi/22_trace_cdata.lua` covers four-level reads and writes.

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

- Deeper than four levels falls back.
- Pointer arithmetic beyond indexing falls back.
- `B == 0` calls and continuation error/yield paths remain unsupported.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
