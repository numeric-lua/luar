# Luar 0.113 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds `(float, int) -> float` native calls.

## Highlights

- `CCALL2FI` uses libffi `float, sint32 -> float` and writes a Lua float.
- `tests/ffi/44_trace_ccall2fi.lua` verifies `scale_fi(2.5, 3) * 1000 =
  7500.0`, compilation and execution.
- No-FFI stub and `ir_is_cdata_function` coverage included.

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

## Known limitations

- FADDI variable-source calls still fall back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
