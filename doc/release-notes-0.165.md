# Luar 0.165 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends runtime integer widening to `double(double,int)` calls.

## Highlights

- `ldexp(i, 1)` executes natively with an integer loop variable.
- The recorder emits `ITOF` for the double argument when its producer is
  proven integer; the integer argument keeps its bytecode proof.
- `tests/ffi/70_trace_loop_int_widen_2di.lua` checks zero entry failures
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
