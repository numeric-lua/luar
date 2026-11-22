# Luar 0.147 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends FADDI arguments to `(int,float)->float` native calls.

## Highlights

- `int_float_scale(2, v + 1)` compiles and executes natively through
  `CCALL2IF`.
- The float argument may be proven by the recorded IR; runtime guards
  continue to validate the integer argument.
- `tests/ffi/55_trace_faddi_2if.lua` checks the result and zero entry
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
