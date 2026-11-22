# Luar 0.168 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends runtime integer widening to `(double,float)->double` calls.

## Highlights

- `double_float_scale(i, 1.5)` executes natively with an integer loop
  variable.
- `ITOF` is emitted for the double argument when its producer is proven
  integer; the float argument keeps its guard.
- `tests/ffi/73_trace_loop_int_widen_2df.lua` checks zero entry failures
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
