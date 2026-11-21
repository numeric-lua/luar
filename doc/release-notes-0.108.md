# Luar 0.108 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
records the verified FADDI C-call case.

## Highlights

- `fabs(2.5 + 1)` inside a loop compiles and executes natively through
  `FADDI → CCALL1DD`.
- A variable FADDI source (`fabs(v + 1)`) stays correct but may fall back at
  execution time.
- `tests/ffi/41_trace_faddi_constant_call.lua` checks both paths.

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

- FADDI call traces with upvalue/parameter sources are not observed to
  execute natively.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
