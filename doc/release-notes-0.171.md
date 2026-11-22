# Luar 0.171 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends runtime integer widening to `(int,double,double)` calls.

## Highlights

- `idd_scale(2, 2.0, i)` executes natively with an integer loop variable.
- `ITOF` is emitted for each double argument whose producer is proven
  integer; the integer argument keeps its tag guard.
- `tests/ffi/76_trace_loop_int_widen_3idd.lua` checks zero entry failures
  and `alloc=0`.

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
