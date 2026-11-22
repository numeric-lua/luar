# Luar 0.151 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends FADDI arguments to `(float,double)->double` native calls.

## Highlights

- `float_double_scale(v + 1, 2.5)` compiles and executes natively.
- The float argument may be proven by the recorded IR.
- `tests/ffi/59_trace_faddi_2fd.lua` checks the result and zero entry
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
