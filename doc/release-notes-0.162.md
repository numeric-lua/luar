# Luar 0.162 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
widens integer upvalue constants for `(float,float)->float` calls.

## Highlights

- `f(a, b)` and `f(a, 4.0)` trace natively when `a`/`b` are closed-over
  integer constants.
- The recorder reads the known ULOAD value and rewrites the slot to `LOADF`.
- `tests/ffi/67_trace_upvalue_int_widen.lua` checks both forms, zero entry
  failures and `alloc=0`.
- Runtime integer variables that change during the loop still fall back.

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
