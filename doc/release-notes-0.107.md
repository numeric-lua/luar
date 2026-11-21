# Luar 0.107 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
fixes float immediates reaching integer C-call paths.

## Highlights

- ADDI consults tracked ULOAD values, so `v + 1` with float `v` records as
  `FADDI` even when the live stack slot is stale.
- `CCALL1DI` is no longer selected when the preceding IR op is float;
  `fabs(v + 1)` returns `3500.0` rather than interpreting float bits as an
  integer.
- `tests/ffi/40_trace_float_addi_call.lua`.
- The call trace may still fall back at execution time; the fix is
  correctness first.

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

- FADDI call traces are not yet observed to execute natively.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
