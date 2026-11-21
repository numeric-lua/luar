# Luar 0.106 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
fixes and enables native float add-immediate traces.

## Highlights

- `FADDI` now decodes the immediate with `GETARG_sC` and passes the double
  constant in `xmm0`, matching the helper ABI.
- `s = s + (v + 1)` with float `v` traces and accumulates correctly
  (`3500.0` for 1000 iterations).
- `tests/jit/104_trace_float_addi_native.lua` checks tracing, execution and
  float result type.

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

- Integer/float dynamic-key writes remain narrow.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
