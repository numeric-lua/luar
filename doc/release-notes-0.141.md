# Luar 0.141 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes FADDI arguments in two-argument native calls.

## Highlights

- `fmin(3.0, v + 1)` and `fmax(3.0, v + 1)` compile and execute natively.
- `CCALL2DD` now accepts any pair of IR-proven float slots, independent of
  argument order.
- `tests/ffi/50_trace_faddi_2arg_rhs.lua` checks both calls and zero entry
  failures.

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
