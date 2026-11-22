# Luar 0.148 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends FADDI arguments to `(float,int)->float` native calls.

## Highlights

- `scale_fi(v + 1, 2)` compiles and executes natively through `CCALL2FI`.
- The float argument may be proven by the recorded IR; runtime guards keep
  validating the integer argument.
- `tests/ffi/56_trace_faddi_2fi.lua` checks the result and zero entry
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
