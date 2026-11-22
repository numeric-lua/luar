# Luar 0.174 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds integer multiply-constant widening.

## Highlights

- `hypotf(i * 2, i)` executes natively with integer loop variables.
- The floatifier accepts integer `MULK/ADDK/SUBK` ops, while float `MULK`
  still converts to `FMULK`.
- `tests/ffi/79_trace_loop_int_widen_mul.lua` checks zero entry failures
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
