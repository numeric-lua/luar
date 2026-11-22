# Luar 0.181 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends runtime widening to mixed integer/float variables.

## Highlights

- `hypotf(i, x)` executes natively with `i` an integer loop variable and
  `x` a float upvalue.
- `ir_slot_is_float` now accepts a `TAG_FLOAT` guard on a slot, so the
  existing `ITOF` fallback can widen the pair.
- `tests/ffi/84_trace_loop_int_float_widen.lua` checks zero entry failures
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
