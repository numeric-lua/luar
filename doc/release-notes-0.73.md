# Luar 0.73 release notes

This release adds native float32 C calls and records the first JIT-on/off
performance snapshot.

## Highlights

- **`CCALL1FF`**: `sqrtf(float)` traces when the argument is a float
  MOVE/LOADF/arithmetic producer; the helper converts Lua doubles to
  `float` and back.
- `tests/ffi/36_trace_ccall1ff.lua` covers constant, parameter, integer
  fallback and differently-typed function fallback.
- `make bench-jit` output for 0.72.0 is recorded in
  `doc/performance.md`, including `call_exit` at 0.45x (JIT slower) and
  the scalar FFI/native-call wins.
- No hard performance gate; regressions stay visible.
- No runtime behavior change.

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
