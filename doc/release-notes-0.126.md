# Luar 0.126 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds `(int, float) -> float` native calls.

## Highlights

- `CCALL2IF` uses libffi `sint32, float -> float`.
- `tests/ffi/45_trace_ccall2if.lua` verifies `int_float_scale(2, 3.5) *
  1000 = 7000.0`, compilation and execution.
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

- FADDI variable-source native calls remain interpreter-side.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
