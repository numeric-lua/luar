# Luar 0.140 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends FADDI argument coverage to two-argument native calls.

## Highlights

- `fmin(v + 1, 3.0)` and `fmax(v + 1, 3.0)` now compile and execute
  natively.
- `CCALL2DD` accepts a first argument proven float by the recorded IR when
  the second argument is a proven double.
- `tests/ffi/49_trace_faddi_2arg.lua` checks both 2-arg calls and zero entry
  failures.

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
