# Luar 0.175 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds regression coverage for integer `ADDK` widening.

## Highlights

- `hypotf(i + 1000, i)` executes natively with integer loop variables.
- Integer `ADDK/SUBK` producers were accepted in 0.174; this release locks
  the end-to-end path with a test.
- `tests/ffi/80_trace_loop_int_widen_addk.lua` checks zero entry failures
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
