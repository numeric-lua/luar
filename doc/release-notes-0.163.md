# Luar 0.163 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds runtime numeric widening for `(float,float)->float` calls.

## Highlights

- `f(i, 4)` and `f(i, 4.0)` trace natively with `i` a loop variable.
- An `ITOF` IR op converts integer slots to floats at runtime; its
  placement is proven by IR tag guards (`TAG_INT`) and float producers.
- `tests/ffi/68_trace_loop_int_widen.lua` checks both forms, zero entry
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
