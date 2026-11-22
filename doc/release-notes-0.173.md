# Luar 0.173 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds regression coverage for negative loop arithmetic widening.

## Highlights

- `hypotf(i, i - 1)` executes natively with integer loop variables.
- Negative `ADDI` results are recognized as integer producers.
- `tests/ffi/78_trace_loop_int_widen_sub.lua` checks zero entry failures
  and `alloc=0`.
- No runtime behavior change; the capability landed with 0.172.

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
