# Luar 0.43 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends pointer-arithmetic writes with one constant offset.

## Highlights

- **Constant-offset integer pointer writes**: `(o.ptr + i + 1)[0] = i`
  recognizes the full pointer-arithmetic sequence and emits `CPSETI`.
- `tests/ffi/22_trace_cdata.lua` covers the integer path.
- The equivalent double-pointer write with a constant offset still falls
  back; direct double pointer writes remain traced.

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

- Constant-offset double pointer writes fall back.
- Multi-step/arbitrary pointer arithmetic falls back.
- `B == 0` calls remain unsupported in prefixes.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
