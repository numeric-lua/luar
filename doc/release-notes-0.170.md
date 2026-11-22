# Luar 0.170 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends runtime integer widening to `(double,int,double)` calls.

## Highlights

- `did_scale(i, 2, 2.0)` executes natively with an integer loop variable.
- `ITOF` is emitted for the first double argument when its producer is
  proven integer; the integer and third-double arguments keep their guards.
- `tests/ffi/75_trace_loop_int_widen_3did.lua` checks zero entry failures
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
