# Luar 0.128 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds `(double, float) -> double` native calls, completing the simple
float/double argument orderings.

## Highlights

- `CCALL2DF` passes a double followed by a float32 parameter.
- `tests/ffi/47_trace_ccall2df.lua` verifies
  `double_float_scale(1.5, 2.5) * 1000 = 3750.0`, compilation and execution.
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
