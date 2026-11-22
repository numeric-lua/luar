# Luar 0.164 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends runtime integer widening to one-argument double calls.

## Highlights

- `fabs(i)` and `sqrt(i)` execute natively with integer loop variables.
- The recorder emits `ITOF` for the proven-integer argument, then
  `CCALL1DD`.
- `tests/ffi/69_trace_loop_int_widen_1dd.lua` checks both calls, zero entry
  failures and `alloc=0`.

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
