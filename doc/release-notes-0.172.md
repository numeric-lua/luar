# Luar 0.172 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends runtime widening to both arguments of `(float,float)->float` calls.

## Highlights

- `hypotf(i, i)` and `hypotf(i, i + 1)` execute natively with integer loop
  variables.
- `ADDI` results are now recognized as integer producers, so simple loop
  arithmetic widens too.
- `tests/ffi/77_trace_loop_int_widen_2ff.lua` checks both forms, zero entry
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
