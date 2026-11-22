# Luar 0.161 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends integer widening to mixed constant-integer/float calls.

## Highlights

- `hypotf(3, 4.0)` and `hypotf(3.0, 4)` compile and execute natively.
- The recorder rewrites only the constant-integer side to `LOADF` and keeps
  the float side as-is.
- `tests/ffi/66_trace_mixed_widen_2ff.lua` covers both orders, zero entry
  failures and `alloc=0`.

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
