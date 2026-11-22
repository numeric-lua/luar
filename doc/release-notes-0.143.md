# Luar 0.143 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
adds regression coverage for FADDI on both two-argument call positions.

## Highlights

- `fmin(v + 1, v + 2)` and `fmax(v + 1, v + 2)` execute natively.
- `tests/ffi/52_trace_faddi_both.lua` checks both calls and zero entry
  failures.
- No runtime behavior change.

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
